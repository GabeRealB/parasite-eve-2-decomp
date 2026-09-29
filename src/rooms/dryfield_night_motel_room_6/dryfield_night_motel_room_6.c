#include "rooms/dryfield_night_motel_room_6.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern UiObjectDesc D_800611E4;

/// `Mc_SaveData[0].state.companionType` (ally present), read through its own symbol.

/// `Mc_SaveData[0].state.at4.loc.view` as it was when the cutscene started, restored
/// when it ends.

/// Area-record patch list applied when the cutscene advances the story flags.

/// Labels of the four entries of the room's save menu: "Save", "Play Data",
/// "Weapon Data" and "PE Data".
extern u8 D_dryfield_night_motel_room_6_80182B50[];
extern u8 D_dryfield_night_motel_room_6_80182B58[];
extern u8 D_dryfield_night_motel_room_6_80182B64[];
extern u8 D_dryfield_night_motel_room_6_80182B70[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_night_motel_room_6_8017D6DC` draws.
extern u8 D_dryfield_night_motel_room_6_80182B78[];
extern u8 D_dryfield_night_motel_room_6_80182BA8[];
extern u8 D_dryfield_night_motel_room_6_80182B80[];
extern u8 D_dryfield_night_motel_room_6_80182B84[];
extern u8 D_dryfield_night_motel_room_6_80182B8C[];
extern u8 D_dryfield_night_motel_room_6_80182B98[];
extern u8 D_dryfield_night_motel_room_6_80182BB0[];
extern u8 D_dryfield_night_motel_room_6_80182BB8[];
extern u8 D_dryfield_night_motel_room_6_80182BC0[];

/// The suffix appended to that panel's count rows.
extern u8 D_dryfield_night_motel_room_6_80182BC8[];

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_night_motel_room_6_80182BD0[];

/// Help texts of the statistics panel's nine rows, handed to the UI holder for
/// the selected row.
extern u8 D_dryfield_night_motel_room_6_80182BD4[];
extern u8 D_dryfield_night_motel_room_6_80182C00[];
extern u8 D_dryfield_night_motel_room_6_80182C24[];
extern u8 D_dryfield_night_motel_room_6_80182C54[];
extern u8 D_dryfield_night_motel_room_6_80182C88[];
extern u8 D_dryfield_night_motel_room_6_80182CBC[];
extern u8 D_dryfield_night_motel_room_6_80182CF4[];
extern u8 D_dryfield_night_motel_room_6_80182D28[];
extern u8 D_dryfield_night_motel_room_6_80182D60[];

/// The play-data menu panel's list.
extern UiList D_dryfield_night_motel_room_6_80182D9C;

/// The usage panel's list.
extern UiList D_dryfield_night_motel_room_6_80182DC4;

/// UI descriptor the play-data panels spawn when they first open.
extern UiObjectDesc D_dryfield_night_motel_room_6_80182DE8;

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_dryfield_night_motel_room_6_80182E04;
extern UiObjectDesc D_dryfield_night_motel_room_6_80182E20;

/// The telephone menu panel's list.
extern UiList D_dryfield_night_motel_room_6_80182E4C;

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
extern u8 D_dryfield_night_motel_room_6_80182E70[];

/// Task table of the room's cutscene: entry 0 is the cutscene task
/// `func_dryfield_night_motel_room_6_801811F0`, entry 1 the sound task
/// `func_dryfield_night_motel_room_6_80181A0C` it runs alongside the scene.
extern TaskDesc D_dryfield_night_motel_room_6_80182E8C[];

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_night_motel_room_6_80182EB0[];

/// Task table whose entries run the story task
/// `func_dryfield_night_motel_room_6_8018189C`.
extern TaskDesc D_dryfield_night_motel_room_6_80182EE0;

/// World position the room's marker is drawn at.
extern SVECTOR D_dryfield_night_motel_room_6_80182EF8[];

/// Area-record patch lists the story task applies as it ends.
extern GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[];
extern GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[];

/// The sound task the cutscene task spawned, killed when the player skips the
/// scene.
extern Task* D_dryfield_night_motel_room_6_801862B4;

/// Script record the room's event handler fills in and hands to the cutscene
/// task as its `spawnArg2`.
extern RoomCutsceneRec D_dryfield_night_motel_room_6_801862B8;

static void func_dryfield_night_motel_room_6_8017F45C(Task* task);
static void func_dryfield_night_motel_room_6_8017F64C(Task* task);
static s32  func_dryfield_night_motel_room_6_80181A9C(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_dryfield_night_motel_room_6_80181C34(Task* task);
static void func_dryfield_night_motel_room_6_80181C78(Task* task);

void func_dryfield_night_motel_room_6_8017D6DC(UiList*, UiObject*);
void func_dryfield_night_motel_room_6_8017DEA8(UiList*, UiObject*);
void func_dryfield_night_motel_room_6_8017E8C0(Task*);
void func_dryfield_night_motel_room_6_8017ED6C(Task*);
void func_dryfield_night_motel_room_6_8017EF2C(Task*);
void func_dryfield_night_motel_room_6_8017F120(UiList*, UiObject*);
void func_dryfield_night_motel_room_6_8017F204(UiList*, UiObject*);
void func_dryfield_night_motel_room_6_8017F2CC(UiList*, UiObject*);
void func_dryfield_night_motel_room_6_8017F394(UiList*, UiObject*);

void func_dryfield_night_motel_room_6_801811F0(Task*);
void func_dryfield_night_motel_room_6_80181A0C(Task*);

void func_dryfield_night_motel_room_6_801811F0(Task*);
void func_dryfield_night_motel_room_6_80181A0C(Task*);

extern GpGridParams   D_dryfield_night_motel_room_6_80183984[1];
extern GpObj4C        D_dryfield_night_motel_room_6_80185A48[10];
extern GpObj4C        D_dryfield_night_motel_room_6_80185D40[15];
extern GpRoomCoordSet D_dryfield_night_motel_room_6_80185A30[1];
s32                   func_dryfield_night_motel_room_6_8018175C(Task*, s32, s32, s32);
s32                   func_dryfield_night_motel_room_6_80181B74(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_dryfield_night_motel_room_6_80181B7C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_dryfield_night_motel_room_6_80181BF8(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_dryfield_night_motel_room_6_80181C00(Task*, s32, s32, GpMessageArg);
void                  func_dryfield_night_motel_room_6_8018189C(Task*);

u8 D_dryfield_night_motel_room_6_80182B50[8] = {
    83, 97, 118, 101, 0, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182B58[12] = {
    80, 108, 97, 121, 32, 68, 97, 116, 97, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182B64[12] = {
    87, 101, 97, 112, 111, 110, 32, 68, 97, 116, 97, 0,
};

u8 D_dryfield_night_motel_room_6_80182B70[8] = {
    80, 69, 32, 68, 97, 116, 97, 0,
};

u8 D_dryfield_night_motel_room_6_80182B78[8] = {
    84, 105, 109, 101, 0, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182B80[4] = {
    87, 111, 110, 0,
};

u8 D_dryfield_night_motel_room_6_80182B84[8] = {
    69, 115, 99, 97, 112, 101, 100, 0,
};

u8 D_dryfield_night_motel_room_6_80182B8C[12] = {
    66, 97, 116, 116, 108, 101, 115, 32, 119, 111, 110, 0,
};

u8 D_dryfield_night_motel_room_6_80182B98[16] = {
    69, 120, 116, 101, 114, 109, 105, 110, 97, 116, 101, 100, 0, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182BA8[8] = {
    83, 97, 118, 101, 100, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182BB0[8] = {
    67, 108, 101, 97, 114, 101, 100, 0,
};

u8 D_dryfield_night_motel_room_6_80182BB8[8] = {
    77, 97, 120, 32, 69, 88, 80, 0,
};

u8 D_dryfield_night_motel_room_6_80182BC0[8] = {
    77, 97, 120, 32, 66, 80, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182BC8[8] = {
    32, 116, 105, 109, 101, 115, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182BD0[4] = {
    37, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182BD4[44] = {
    84, 111, 116, 97, 108, 32, 97, 109, 111, 117, 110, 116, 32, 111, 102, 10,
    116, 105, 109, 101, 32, 115, 112, 101, 110, 116, 32, 102, 111, 114, 32, 116,
    104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182C00[36] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 115, 97, 118, 101, 115, 10,
    117, 115, 101, 100, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182C24[48] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 110, 101, 109, 105, 101, 115, 10, 100, 101, 102, 101, 97, 116, 101, 100,
    32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182C54[52] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 115, 99, 97, 112, 101, 115, 10, 102, 114, 111, 109, 32, 98, 97, 116,
    116, 108, 101, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101,
    46, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182C88[52] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 98, 97, 116, 116, 108, 101, 115,
    32, 119, 111, 110, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182CBC[56] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 101, 110, 101, 109, 105, 101, 115,
    32, 100, 101, 102, 101, 97, 116, 101, 100, 32, 105, 110, 32, 116, 104, 105,
    115, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_dryfield_night_motel_room_6_80182CF4[52] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 116, 105, 109, 101, 115, 32,
    121, 111, 117, 32, 104, 97, 118, 101, 10, 99, 108, 101, 97, 114, 101, 100,
    32, 116, 104, 101, 32, 103, 97, 109, 101, 32, 115, 111, 32, 102, 97, 114,
    46, 0, 0, 0,
};

u8 D_dryfield_night_motel_room_6_80182D28[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 69, 88, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10,
    98, 121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104,
    101, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_dryfield_night_motel_room_6_80182D60[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 66, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10, 98,
    121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104, 101,
    32, 103, 97, 109, 101, 46, 0, 0,
};

UiListItemFunc D_dryfield_night_motel_room_6_80182D98[1] = {
    func_dryfield_night_motel_room_6_8017D6DC,
};

UiList D_dryfield_night_motel_room_6_80182D9C = { D_dryfield_night_motel_room_6_80182D98, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_dryfield_night_motel_room_6_80182DC0[1] = {
    func_dryfield_night_motel_room_6_8017DEA8,
};

UiList D_dryfield_night_motel_room_6_80182DC4 = { D_dryfield_night_motel_room_6_80182DC0, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_dryfield_night_motel_room_6_80182DE8 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_dryfield_night_motel_room_6_8017ED6C, 0 };

UiObjectDesc D_dryfield_night_motel_room_6_80182E04 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_dryfield_night_motel_room_6_8017EF2C, 0 };

UiObjectDesc D_dryfield_night_motel_room_6_80182E20 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_dryfield_night_motel_room_6_8017E8C0, 0 };

UiListItemFunc D_dryfield_night_motel_room_6_80182E3C[4] = {
    func_dryfield_night_motel_room_6_8017F120,
    func_dryfield_night_motel_room_6_8017F204,
    func_dryfield_night_motel_room_6_8017F2CC,
    func_dryfield_night_motel_room_6_8017F394,
};

UiList D_dryfield_night_motel_room_6_80182E4C = { D_dryfield_night_motel_room_6_80182E3C, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

u8 D_dryfield_night_motel_room_6_80182E70[4] = {
    12,
    8,
    12,
    8,
};

void func_dryfield_night_motel_room_6_80180FD0(Task*);

TaskDesc D_dryfield_night_motel_room_6_80182E74[2] = {
    { 0, 112, func_dryfield_night_motel_room_6_801811A0, { .model = NULL } },
    { 0, 112, func_dryfield_night_motel_room_6_80180FD0, { .model = NULL } },
};

TaskDesc D_dryfield_night_motel_room_6_80182E8C[3] = {
    { 0, 32, func_dryfield_night_motel_room_6_801811F0, { .model = NULL } },
    { 0, 32, func_dryfield_night_motel_room_6_80181A0C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_night_motel_room_6_80182EB0[6] = {
    { 5102, func_dryfield_night_motel_room_6_80181B7C },
    { 5105, func_dryfield_night_motel_room_6_80181B74 },
    { 5103, func_dryfield_night_motel_room_6_80181BF8 },
    { 5104, func_dryfield_night_motel_room_6_8018175C },
    { 5106, func_dryfield_night_motel_room_6_80181C00 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_night_motel_room_6_80182EE0 = { 0, 32, func_dryfield_night_motel_room_6_8018189C, { .model = NULL } };

TaskDesc D_dryfield_night_motel_room_6_80182EEC = { 0, 192, func_dryfield_night_motel_room_6_8018189C, { .model = NULL } };

SVECTOR D_dryfield_night_motel_room_6_80182EF8[1] = {
    { 550, -850, 5170, 0 },
};

GpRoomCoordRec D_dryfield_night_motel_room_6_80182F00[1] = {
    { D_dryfield_night_motel_room_6_80185A30, NULL },
};

GpRoomObjRec D_dryfield_night_motel_room_6_80182F08[1] = {
    { D_dryfield_night_motel_room_6_80183984, D_dryfield_night_motel_room_6_80185A48, D_dryfield_night_motel_room_6_80185D40, NULL },
};

u8 * D_dryfield_night_motel_room_6_80182F18[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_motel_room_6_80182F1C[1] = {
    { { .bytes = { 12, 0 } } },
};

GpWarpRec D_dryfield_night_motel_room_6_80182F20[2] = {
    { { .words = { 3072, 4350, 0, 1500 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4350, 0, 1500 } }, { 0, 0, 0, 0 }, 0x531E0002, 0x531E0001, 0, 2, 0, 465 },
    { { .words = { 1024, 715, 0, 6640 } }, { 0, 0, 0, 0 }, { .words = { 1024, 715, 0, 6640 } }, { 0, 0, 0, 0 }, 0, 0x531E0003, 0, 5, 2, 0 },
};

SVECTOR D_dryfield_night_motel_room_6_80182F90[24] = {
    { -3422, 0, 2250, 0 },
    { 0, 0, 4096, 0 },
    { 3422, 0, 2250, 0 },
    { -4096, 0, 0, 0 },
    { 4096, 0, 0, 0 },
    { -318, 0, -4084, 0 },
    { 560, 0, -4057, 0 },
    { 4096, 0, 0, 0 },
    { 1512, 0, -3807, 0 },
    { 0, -4096, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 3501, 0, -2125, 0 },
    { -2882, 0, -2911, 0 },
    { -4050, 0, -614, 0 },
    { -353, 0, -4081, 0 },
    { 2283, 0, 3401, 0 },
    { 4096, 0, 0, 0 },
    { -390, 0, 4077, 0 },
    { -2713, 0, 3069, 0 },
    { -782, 0, 4021, 0 },
    { -4085, 0, 295, 0 },
    { 0, 4096, 0, 0 },
    { 4094, 0, -127, 0 },
};

SVECTOR D_dryfield_night_motel_room_6_80183050[158] = {
    { 2472, -5, 721, 0 },
    { 2472, -847, 721, 0 },
    { 2129, -847, 200, 0 },
    { 2129, -5, 200, 0 },
    { 3336, -5, 721, 0 },
    { 3336, -847, 721, 0 },
    { 3679, -5, 200, 0 },
    { 3679, -847, 200, 0 },
    { 2183, -5, 6300, 0 },
    { 2183, -2150, 6300, 0 },
    { 2183, -2150, 5950, 0 },
    { 2183, -5, 5950, 0 },
    { 4800, -5, 4904, 0 },
    { 4800, -3000, 4904, 0 },
    { 4800, -3000, 100, 0 },
    { 4800, -5, 100, 0 },
    { 4800, -5, 7900, 0 },
    { 4800, -3000, 7900, 0 },
    { 2700, -2000, 4800, 0 },
    { 2700, -3000, 4800, 0 },
    { 2700, -3000, 6000, 0 },
    { 2700, -2000, 6000, 0 },
    { 1600, -5, 7800, 0 },
    { 1600, -1250, 7800, 0 },
    { 1600, -1250, 7210, 0 },
    { 1600, -5, 7210, 0 },
    { 2500, -1250, 7140, 0 },
    { 2500, -5, 7140, 0 },
    { 2592, -5, 5977, 0 },
    { 2592, -1194, 5977, 0 },
    { 3148, -1194, 6054, 0 },
    { 3148, -5, 6054, 0 },
    { 2350, -2000, 6000, 0 },
    { 2350, -3000, 6000, 0 },
    { 2350, -3000, 4800, 0 },
    { 2350, -2000, 4800, 0 },
    { 800, -5, 3764, 0 },
    { 800, -800, 3764, 0 },
    { 800, -800, 6000, 0 },
    { 800, -5, 6000, 0 },
    { 2210, -5, 8000, 0 },
    { 2210, -3000, 8000, 0 },
    { 2210, -3000, 7800, 0 },
    { 2210, -5, 7800, 0 },
    { 471, -5, 6037, 0 },
    { 471, -1050, 6037, 0 },
    { 471, -1050, 7800, 0 },
    { 471, -5, 7800, 0 },
    { 567, -5, 4950, 0 },
    { 567, -550, 4950, 0 },
    { 1020, -550, 5130, 0 },
    { 1020, -5, 5130, 0 },
    { 1020, -550, 6060, 0 },
    { 1020, -5, 6060, 0 },
    { 200, -550, 3836, 0 },
    { 2523, -550, 3836, 0 },
    { 2523, -550, 2164, 0 },
    { 200, -550, 2164, 0 },
    { 200, -5, 2164, 0 },
    { 2523, -5, 2164, 0 },
    { 2523, -5, 3836, 0 },
    { 200, -5, 3836, 0 },
    { 4900, -5, 200, 0 },
    { 4900, -3000, 200, 0 },
    { 100, -3000, 200, 0 },
    { 100, -5, 200, 0 },
    { 3201, -5, 4484, 0 },
    { 3201, -901, 4484, 0 },
    { 3366, -901, 4756, 0 },
    { 3366, -5, 4756, 0 },
    { 2621, -5, 4484, 0 },
    { 2621, -901, 4484, 0 },
    { 2346, -5, 4756, 0 },
    { 2346, -901, 4756, 0 },
    { 0, -5, 6037, 0 },
    { 0, -1050, 6037, 0 },
    { 4800, -5, 5000, 0 },
    { 4800, -3000, 5000, 0 },
    { 2308, -3000, 5000, 0 },
    { 2308, -5, 5000, 0 },
    { 650, -7, 200, 0 },
    { 650, -2000, 200, 0 },
    { 650, -2000, 2218, 0 },
    { 650, -5, 2218, 0 },
    { 3977, -5, 4800, 0 },
    { 3977, -2000, 4800, 0 },
    { 4150, -2000, 3658, 0 },
    { 4150, -5, 3658, 0 },
    { 4800, -2000, 3602, 0 },
    { 4800, -5, 3602, 0 },
    { 2308, -5, 4650, 0 },
    { 2308, -3000, 4650, 0 },
    { 4800, -3000, 4650, 0 },
    { 4800, -5, 4650, 0 },
    { 1173, -5, 5950, 0 },
    { 1173, -2150, 5950, 0 },
    { 1173, -2150, 6300, 0 },
    { 1173, -5, 6300, 0 },
    { 3172, -5, 6826, 0 },
    { 3172, -1194, 6826, 0 },
    { 2592, -1194, 7215, 0 },
    { 2592, -5, 7215, 0 },
    { 200, -5, 5950, 0 },
    { 200, -2150, 5950, 0 },
    { 200, -5, 7800, 0 },
    { 200, -1050, 7800, 0 },
    { 2500, -1050, 7800, 0 },
    { 2500, -5, 7800, 0 },
    { 200, -5, 100, 0 },
    { 200, -3000, 100, 0 },
    { 200, -3000, 6200, 0 },
    { 200, -5, 6200, 0 },
    { 4332, -5, 5257, 0 },
    { 4332, -1000, 5257, 0 },
    { 2802, -1000, 5111, 0 },
    { 2802, -5, 5111, 0 },
    { 2620, -1000, 4950, 0 },
    { 2620, -5, 4950, 0 },
    { 4800, -5, 6276, 0 },
    { 4800, -466, 6276, 0 },
    { 4029, -466, 6126, 0 },
    { 4029, -5, 6126, 0 },
    { 3944, -466, 4950, 0 },
    { 3944, -5, 4950, 0 },
    { 2860, -5, 6070, 0 },
    { 2860, -3000, 6070, 0 },
    { 2860, -3000, 7800, 0 },
    { 2860, -5, 7800, 0 },
    { 2500, -2150, 6200, 0 },
    { 200, -2150, 6200, 0 },
    { 200, -2150, 6000, 0 },
    { 2500, -2150, 6000, 0 },
    { 5000, -3000, 8000, 0 },
    { 0, -3000, 8000, 0 },
    { 0, -3000, 0, 0 },
    { 5000, -3000, 0, 0 },
    { 2500, -5, 6300, 0 },
    { 2500, -2150, 6300, 0 },
    { 2210, -3000, 6200, 0 },
    { 2210, -5, 6200, 0 },
    { 200, -2150, 6300, 0 },
    { 200, -5, 6300, 0 },
    { 2700, -2150, 5950, 0 },
    { 2700, -5, 5950, 0 },
    { 4900, -3000, 7800, 0 },
    { 4900, -5, 7800, 0 },
    { 2635, 0, 6163, 0 },
    { 2635, 0, 8000, 0 },
    { 4990, 0, 8000, 0 },
    { 4990, 0, 6163, 0 },
    { 0, 0, 6163, 0 },
    { 0, 0, 8000, 0 },
    { 4990, 0, 4987, 0 },
    { 4990, 0, 0, 0 },
    { 2635, 0, 0, 0 },
    { 2635, 0, 4987, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 4987, 0 },
};

GpGridFace D_dryfield_night_motel_room_6_80183540[56] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 1, 4, 0 }, 1, 0 },
    { { 7, 5, 6, 4 }, 2, 0 },
    { { 9, 10, 8, 11 }, 3, 0 },
    { { 13, 14, 12, 15 }, 3, 0 },
    { { 12, 16, 13, 17 }, 3, 0 },
    { { 19, 20, 18, 21 }, 4, 0 },
    { { 23, 24, 22, 25 }, 3, 0 },
    { { 24, 26, 25, 27 }, 5, 0 },
    { { 29, 30, 28, 31 }, 6, 0 },
    { { 33, 34, 32, 35 }, 3, 0 },
    { { 37, 38, 36, 39 }, 7, 0 },
    { { 41, 42, 40, 43 }, 3, 0 },
    { { 45, 46, 44, 47 }, 4, 0 },
    { { 49, 50, 48, 51 }, 8, 0 },
    { { 50, 52, 51, 53 }, 4, 0 },
    { { 55, 56, 54, 57 }, 9, 0 },
    { { 57, 56, 58, 59 }, 10, 0 },
    { { 56, 55, 59, 60 }, 4, 0 },
    { { 55, 54, 60, 61 }, 1, 0 },
    { { 63, 64, 62, 65 }, 11, 0 },
    { { 67, 68, 66, 69 }, 12, 0 },
    { { 71, 67, 70, 66 }, 10, 0 },
    { { 73, 71, 72, 70 }, 13, 0 },
    { { 75, 45, 74, 44 }, 10, 0 },
    { { 77, 78, 76, 79 }, 1, 0 },
    { { 81, 82, 80, 83 }, 4, 0 },
    { { 85, 86, 84, 87 }, 14, 0 },
    { { 86, 88, 87, 89 }, 15, 0 },
    { { 91, 92, 90, 93 }, 10, 0 },
    { { 95, 96, 94, 97 }, 4, 0 },
    { { 99, 100, 98, 101 }, 16, 0 },
    { { 103, 95, 102, 94 }, 10, 0 },
    { { 105, 106, 104, 107 }, 10, 0 },
    { { 109, 110, 108, 111 }, 17, 0 },
    { { 113, 114, 112, 115 }, 18, 0 },
    { { 78, 91, 79, 90 }, 3, 0 },
    { { 114, 116, 115, 117 }, 19, 0 },
    { { 119, 120, 118, 121 }, 20, 0 },
    { { 120, 122, 121, 123 }, 21, 0 },
    { { 125, 126, 124, 127 }, 4, 0 },
    { { 129, 130, 128, 131 }, 22, 0 },
    { { 32, 35, 21, 18 }, 22, 0 },
    { { 133, 134, 132, 135 }, 22, 0 },
    { { 137, 9, 136, 8 }, 1, 0 },
    { { 30, 99, 31, 98 }, 23, 0 },
    { { 42, 138, 43, 139 }, 3, 0 },
    { { 96, 140, 97, 141 }, 1, 0 },
    { { 10, 142, 11, 143 }, 10, 0 },
    { { 42, 144, 43, 145 }, 10, 0 },
    { { 147, 148, 146, 149 }, 9, 3 },
    { { 151, 147, 150, 146 }, 9, 2 },
    { { 153, 154, 152, 155 }, 9, 1 },
    { { 146, 149, 155, 152 }, 9, 3 },
    { { 154, 156, 155, 157 }, 9, 1 },
    { { 150, 146, 157, 155 }, 9, 1 },
};

s16 D_dryfield_night_motel_room_6_801837E0[41] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    9,
    10,
    11,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    25,
    26,
    27,
    28,
    29,
    30,
    32,
    34,
    35,
    36,
    37,
    39,
    41,
    42,
    43,
    48,
    52,
    53,
    54,
    55,
    -1,
};

s16 D_dryfield_night_motel_room_6_80183834[53] = {
    3,
    4,
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
    15,
    16,
    17,
    18,
    19,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    -1,
};

s16 D_dryfield_night_motel_room_6_801838A0[28] = {
    3,
    5,
    6,
    7,
    8,
    9,
    10,
    12,
    13,
    15,
    30,
    31,
    33,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    53,
    55,
    -1,
};

s16 D_dryfield_night_motel_room_6_801838D8[23] = {
    0,
    1,
    2,
    4,
    5,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    25,
    27,
    28,
    29,
    35,
    39,
    43,
    52,
    53,
    54,
    -1,
};

s16 D_dryfield_night_motel_room_6_80183908[40] = {
    3,
    4,
    5,
    6,
    8,
    9,
    10,
    16,
    18,
    19,
    21,
    22,
    23,
    25,
    27,
    28,
    29,
    31,
    33,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    -1,
};

s16 D_dryfield_night_motel_room_6_80183958[9] = {
    5,
    38,
    40,
    43,
    49,
    50,
    51,
    53,
    -1,
};

s16 * D_dryfield_night_motel_room_6_8018396C[6] = {
    D_dryfield_night_motel_room_6_801837E0,
    D_dryfield_night_motel_room_6_80183834,
    D_dryfield_night_motel_room_6_801838A0,
    D_dryfield_night_motel_room_6_801838D8,
    D_dryfield_night_motel_room_6_80183908,
    D_dryfield_night_motel_room_6_80183958,
};

GpGridParams D_dryfield_night_motel_room_6_80183984[1] = {
    { NULL, D_dryfield_night_motel_room_6_80182F90, D_dryfield_night_motel_room_6_80183050, D_dryfield_night_motel_room_6_80183540, D_dryfield_night_motel_room_6_8018396C, 0, 0, 2, 3, 4000, 56 },
};

GpViewRec D_dryfield_night_motel_room_6_801839A8[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x3656, -4000 } }, 289 },
    { { { { -3958, 0, -1051 }, { -55, 4090, 209 }, { 1049, 216, -3953 } }, { -1150, 1360, -5850 } }, 221 },
    { { { { 3963, 0, -1033 }, { -53, 4090, -206 }, { 1032, 213, 3957 } }, { -1200, 1360, -200 } }, 216 },
    { { { { 3992, 0, -914 }, { -253, 3935, -1106 }, { 879, 1134, 3836 } }, { -1000, 1960, -2700 } }, 246 },
    { { { { -567, 0, -4056 }, { -3575, 1934, 500 }, { 1916, 3610, -268 } }, { 750, 4410, -7350 } }, 235 },
    { { { { 358, 0, -4080 }, { -98, 4094, -8 }, { 4079, 98, 358 } }, { -200, 1060, -5350 } }, 263 },
    { { { { -4088, 0, -255 }, { -145, 3367, 2327 }, { 210, 2331, -3360 } }, { -3350, 2540, -7800 } }, 257 },
    { { { { 4092, 0, -163 }, { -99, 3249, -2491 }, { 129, 2493, 3246 } }, { -3350, 2390, -5000 } }, 246 },
    { { { { -3961, 0, -1041 }, { -524, 3537, 1997 }, { 899, 2065, -3421 } }, { -2523, 1553, -1908 } }, 282 },
    { { { { 1777, 0, 3690 }, { -2798, 2670, 1347 }, { -2405, -3105, 1158 } }, { -1947, 0x2BC9, 3461 } }, 257 },
    { { { { 1823, 0, 3667 }, { 2, 4096, -1 }, { -3667, 2, 1823 } }, { -7945, 4655, 5950 } }, 230 },
    { { { { 887, 0, 3998 }, { 2647, 3069, -587 }, { -2996, 2711, 665 } }, { -1401, 1658, -4928 } }, 680 },
};

GpSprtCmd D_dryfield_night_motel_room_6_80183B58[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_room_6_80183B68[56] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 0, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 72, 0, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 56, 0, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, -48, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -48, 0, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 32, 487, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, -40, 474, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 32, 443, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 56, 490, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 72, 471, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 460, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 0, 497, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -48, 406, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -64, 0, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 16, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -48, 112, 0, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 32 } }, -16, 0, 0, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 0, 0, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 16, 0, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 0, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 48, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 0, 926, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 929, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 24, 722, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 686, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 24, 627, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 16, 627, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 40, 614, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 0, 784, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 16, 680, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 485, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 24, 605, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 128, 48, 531, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 112, 500, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 32, 594, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, 72, 496, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, 32, 649, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 72, 523, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 16, 32, 708, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 16, 64, 548, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 112, 536, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 32, 728, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 40, 602, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 48, 555, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 56, 567, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 72, 634, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 96, 634, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, 64, 559, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 32, 718, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, 56, 569, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -24, 64, 578, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 72, 583, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 64, 601, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 80, 588, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 819, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 40, 613, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_room_6_80183FC8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 43, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_room_6_80183FE8[47] = {
    { 142, 0x3FC0, { .fields = { 184, 40 } }, -120, -40, 0, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 176, 8 } }, -112, 0, 0, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, 8, 0, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 16, 0, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -56, 112, 0, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 104, 0, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 24 } }, -112, 8, 0, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 0, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 8, 905, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 601, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 589, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 593, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 80, 596, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 32, 619, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 24, 704, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 24, 739, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 16, 839, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 24, 888, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 0, 700, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 371, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 0, 0xAF28, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 48, 373, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 371, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -120, 88, 490, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, 48, 516, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 32, 642, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -56, 32, 700, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -24, 32, 700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 64, 579, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 56, 566, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 48, 578, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 572, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 40, 613, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -24, 48, 591, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -56, 48, 594, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, 40, 636, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 522, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -80, 64, 517, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -40, 64, 537, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -8, 64, 559, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 24, 88, 563, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 80, 564, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 567, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 561, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, 88, 536, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, 88, 516, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 509, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_room_6_80184394[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_room_6_801843AC[43] = {
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -144, 16, 809, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -144, 32, 693, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 48, 621, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 56, 622, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 64, 618, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 72, 618, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 80, 600, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -144, 80, 375, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 160 } }, -120, -88, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -88, 825, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -64, 825, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 825, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 834, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -16, 837, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 900, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 16, 900, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 904, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, 48, 900, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 48, 680, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -88, 64, 680, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -88, 72, 679, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 80, 715, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 232 } }, 128, -120, 475, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 72, -120, 693, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 144 } }, 80, -120, 612, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 144 } }, 88, -120, 587, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 144 } }, 96, -120, 587, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 104, -120, 582, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 112, -120, 583, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 120, -120, 583, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 96, 24, 587, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 88, 425, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 88, 662, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 662, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 24, 590, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 24, 592, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 24, 593, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 64, 425, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 64, 662, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 24, 615, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, 64, 631, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 24, 615, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 64, 631, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_room_6_80184708[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 10, 0, 0, { 0, 0 } },
    { 18, 4, 0, 0, { 2, 0 } },
    { 22, 21, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_room_6_80184738[29] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, -120, 0, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -80, 0, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -56, 0, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 24, -112, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -88, 0, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, -56, 0, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 0, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -8, 0, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 64, -8, 500, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 56, -32, 500, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 48, -40, 500, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 48, -56, 500, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, -80, 500, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, -88, 500, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, -120, 500, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 32, -112, 500, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -48, -64, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -24, 1500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -40, 930, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -64, 930, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 48, 0, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 450, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, 48, 500, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, 48, 500, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -56, 48, 500, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 32, 0, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 40, 450, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 16, 32, 450, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 32, 450, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_motel_room_6_8018497C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 2, 0 } },
    { 20, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_room_6_801849A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_room_6_801849B4[24] = {
    { 142, 0x3FC0, { .fields = { 32, 184 } }, 112, -64, 300, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 184 } }, 80, -64, 287, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 88 } }, 24, -64, 0x30D4, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 24, 512, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 24, 32, 553, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 40, 560, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 24, 48, 542, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 56, 539, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 64, 537, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 72, 534, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 80, 532, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 88, 366, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 96, 325, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 24, 104, 312, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 24, 1250, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 40, 710, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 48, 705, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 56, 709, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 706, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 72, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, -128, 24, 1250, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 32, 710, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 240 } }, 88, -120, 310, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 120, 240 } }, 40, -120, 200, { .fields = { 8, 0 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_dryfield_night_motel_room_6_80184B94[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 8, 0, 0, { 0, 0 } },
    { 22, 1, 0, 0, { 2, 0 } },
    { 23, 1, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_motel_room_6_80184BC4[147] = {
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -136, 0, 0, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 0, 489, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 40, 0, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 24, 0, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 0, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -136, 8, 360, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -136, 24, 368, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -136, 40, 367, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, 56, 358, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -64, -112, 0, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -56, -64, 0, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -16, 0, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, 40, 0, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -64, 104, 0, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 0, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -160, 24, 376, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -48, 303, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -120, 308, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, -16, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -48, 8, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -48, 40, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -120, 24, 406, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, -80, 412, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -120, -120, 429, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, -120, 0, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -120, 625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, -80, 663, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 32, 1250, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 56, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 80, 1250, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, 96, 1250, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, 104, 556, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -8, 606, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -8, 827, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 0, 807, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 834, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 8, 483, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 16, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, -24, 811, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, -32, 768, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -32, 612, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -32, 447, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -24, 472, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 353, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 16, 432, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 0, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, -32, 0, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -32, 0, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -16, 609, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 0, 497, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 8, 0, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -104, -24, 0, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -40, 781, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -48, 818, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -56, 743, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -64, 733, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -64, 702, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -56, 692, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -48, 732, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -40, 711, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -16, 777, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -16, 817, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 0, 0, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, 80, 0, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 0, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, 0, 0, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 8, 569, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 16, 584, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 564, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 545, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 528, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 512, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 56, 498, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 484, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, 72, 451, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -72, 16, 584, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -72, 24, 573, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -72, 72, 431, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -96, 96, 454, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 428, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -112, 72, 406, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 64, 400, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 56, 405, { .fields = { 104, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -112, 32, 423, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -96, 24, 465, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -96, 16, 483, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -64, 8, 534, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, 0, 507, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 515, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 511, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 468, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -80, 64, 452, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 452, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -112, 48, 419, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 48, 510, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 40, 541, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 40, 433, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 32, 511, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 550, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 96, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, 104, 375, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 72, 96, 375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -32, -40, 0, { .fields = { 40, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -104, 8, 0, { .fields = { 24, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -104, -24, 0, { .fields = { 0, 216 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -104, -64, 0, { .fields = { 16, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 64, 32 } }, -104, -120, 429, { .fields = { 88, 184 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 64, 24 } }, -104, -88, 410, { .fields = { 56, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -32, -120, 0, { .fields = { 88, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -40, -120, 0, { .fields = { 72, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -80, -8, 549, { .fields = { 56, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 16, 1250, { .fields = { 0, 64 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 24, 1250, { .fields = { 0, 72 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 32, 1250, { .fields = { 8, 248 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 40, 1250, { .fields = { 8, 96 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 48, 1250, { .fields = { 8, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -72, 8, 530, { .fields = { 32, 160 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -56, 8, 574, { .fields = { 32, 200 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -40, 8, 530, { .fields = { 32, 152 } }, 128, 128, 128, 2 },
    { 141, 0x4000, { .fields = { 56, 8 } }, -88, -64, 463, { .fields = { 120, 120 } }, 128, 128, 128, 2 },
    { 141, 0x4000, { .fields = { 56, 8 } }, -88, -56, 488, { .fields = { 120, 128 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, -48, 482, { .fields = { 8, 136 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, -40, 493, { .fields = { 8, 144 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, -32, 472, { .fields = { 8, 112 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -80, -24, 469, { .fields = { 16, 32 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -80, -16, 529, { .fields = { 16, 104 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -144, 8, 0, { .fields = { 104, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -96, 16, 0, { .fields = { 96, 48 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 8 } }, -120, 16, 388, { .fields = { 32, 88 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -120, 8, 407, { .fields = { 16, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, -8, 0, { .fields = { 16, 80 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -128, 0, 374, { .fields = { 8, 48 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -128, -8, 390, { .fields = { 16, 56 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -88, -72, 0, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -96, -72, 0, { .fields = { 24, 40 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -104, -72, 0, { .fields = { 72, 176 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -136, -16, 376, { .fields = { 40, 40 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -144, -32, 0, { .fields = { 48, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 8 } }, -136, -24, 377, { .fields = { 80, 248 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -136, -32, 377, { .fields = { 24, 224 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -144, -40, 351, { .fields = { 24, 216 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -136, -48, 365, { .fields = { 24, 8 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -128, -56, 380, { .fields = { 32, 16 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 8 } }, -128, -64, 366, { .fields = { 40, 24 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 8 } }, -128, -72, 373, { .fields = { 96, 104 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -136, -72, 0, { .fields = { 112, 32 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -144, -72, 0, { .fields = { 80, 144 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_dryfield_night_motel_room_6_80185740[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 3, 0 } },
    { 9, 53, 0, 0, { 0, 0 } },
    { 62, 37, 0, 0, { 5, 0 } },
    { 99, 3, 0, 0, { 1, 0 } },
    { 102, 24, 0, 0, { 4, 0 } },
    { 126, 21, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_room_6_80185780[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_room_6_80185790[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_room_6_801857A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_motel_room_6_801857B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_motel_room_6_801857C0[12] = {
    { { .empty = D_dryfield_night_motel_room_6_80183B58 }, D_dryfield_night_motel_room_6_80183B58, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80183B68 }, D_dryfield_night_motel_room_6_80183FC8, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80183FE8 }, D_dryfield_night_motel_room_6_80184394, NULL },
    { { .elements = D_dryfield_night_motel_room_6_801843AC }, D_dryfield_night_motel_room_6_80184708, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80184738 }, D_dryfield_night_motel_room_6_8018497C, NULL },
    { { .empty = D_dryfield_night_motel_room_6_801849A4 }, D_dryfield_night_motel_room_6_801849A4, NULL },
    { { .elements = D_dryfield_night_motel_room_6_801849B4 }, D_dryfield_night_motel_room_6_80184B94, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80184BC4 }, D_dryfield_night_motel_room_6_80185740, NULL },
    { { .empty = D_dryfield_night_motel_room_6_80185780 }, D_dryfield_night_motel_room_6_80185780, NULL },
    { { .empty = D_dryfield_night_motel_room_6_80185790 }, D_dryfield_night_motel_room_6_80185790, NULL },
    { { .empty = D_dryfield_night_motel_room_6_801857A0 }, D_dryfield_night_motel_room_6_801857A0, NULL },
    { { .empty = D_dryfield_night_motel_room_6_801857B0 }, D_dryfield_night_motel_room_6_801857B0, NULL },
};

GpPointLight D_dryfield_night_motel_room_6_80185850[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -882, -3651, 0x2A8A } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 528, 586, 603, { 0, 0 } }, 0x32C8, 0x4650 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2452, -1396, 2755 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3173, 3151, 3128, { 0, 0 } }, 2120, 4223 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4822, -1215, 4142 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } }, 0, 478 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 429, -1248, 4815 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4094, 3618, 3115, { 0, 0 } }, 1800, 2981 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3295, -1382, 6280 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3340, 3277, 3227, { 0, 0 } }, 652, 2681 },
};

GpRoomCoordSet D_dryfield_night_motel_room_6_80185A30[1] = {
    { 0, NULL, 5, D_dryfield_night_motel_room_6_80185850, 0, NULL },
};

GpObj4C D_dryfield_night_motel_room_6_80185A48[10] = {
    { NULL, NULL, NULL, { 3590, -1168, 3040, 0 }, { { -1702, -2192, 0, 0 }, { 1690, -2192, 0, 0 }, { -1702, 2192, 0, 0 }, { 1716, 2192, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 3743, -1104, 2911, 0 }, { { 1727, -2128, 51, 0 }, { -1726, -2128, -50, 0 }, { 1727, 2128, 51, 0 }, { -1726, 2128, -50, 0 } }, { -120, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 1215, -1088, 4559, 0 }, { { 1311, -2112, 99, 0 }, { -1310, -2112, -98, 0 }, { 1311, 2112, 99, 0 }, { -1310, 2112, -98, 0 } }, { -309, 0, 4084, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 1247, -1200, 4735, 0 }, { { -1304, -2224, -163, 0 }, { 1304, -2224, 163, 0 }, { -1304, 2224, -163, 0 }, { 1304, 2224, 163, 0 } }, { 508, 0, -4070, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 1439, -1248, 6239, 0 }, { { -1267, -2272, -353, 0 }, { 1266, -2272, 352, 0 }, { -1267, 2272, -353, 0 }, { 1266, 2272, 352, 0 } }, { 1101, 0, -3960, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 1343, -1296, 6079, 0 }, { { 1266, -2320, 352, 0 }, { -1266, -2320, -352, 0 }, { 1266, 2320, 352, 0 }, { -1266, 2320, -352, 0 } }, { -1099, 0, 3949, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 2654, -1296, 5439, 0 }, { { -1, -2320, 639, 0 }, { 1, -2320, -639, 0 }, { -1, 2320, 639, 0 }, { 1, 2320, -639, 0 } }, { -4111, 0, -9, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 4, 7, 1, 0 },
    { NULL, NULL, NULL, { 2527, -1376, 5407, 0 }, { { 33, -2400, -638, 0 }, { -32, -2400, 639, 0 }, { 33, 2400, -638, 0 }, { -32, 2400, 639, 0 } }, { 4103, 0, 208, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 3455, -1248, 6365, 0 }, { { -627, -2272, 124, 0 }, { 627, -2272, -123, 0 }, { -627, 2272, 124, 0 }, { 627, 2272, -123, 0 } }, { -797, 0, -4032, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 3392, -1312, 6237, 0 }, { { 627, -2336, -123, 0 }, { -627, -2336, 124, 0 }, { 627, 2336, -123, 0 }, { -627, 2336, 124, 0 } }, { 791, 0, 4028, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 8, 7, 129, 0 },
};

GpObj4C D_dryfield_night_motel_room_6_80185D40[15] = {
    { NULL, NULL, NULL, { 4512, -48, 1328, 0 }, { { -320, 0, -784, 0 }, { 320, 0, -784, 0 }, { -320, 0, 784, 0 }, { 320, 0, 784, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 846, 0, 29, 19, 2, 0 },
    { NULL, NULL, NULL, { 896, -64, 1199, 0 }, { { -320, 0, -1088, 0 }, { 320, 0, -1088, 0 }, { -320, 0, 1089, 0 }, { 320, 0, 1089, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1130, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { 1304, -64, 7248, 0 }, { { -504, 0, -1088, 0 }, { -216, 0, -1088, 0 }, { -504, 0, 1088, 0 }, { 1224, 0, 1088, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1634, 0x8005, 0, 0, 3, 0 },
    { NULL, NULL, NULL, { 3328, -64, 4384, 0 }, { { -704, 0, -400, 0 }, { 704, 0, -400, 0 }, { -704, 0, 400, 0 }, { 704, 0, 400, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 809, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 1264, -64, 3024, 0 }, { { -1776, 0, -1280, 0 }, { 1776, 0, -1280, 0 }, { -1776, 0, 1280, 0 }, { 1776, 0, 1280, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2187, 2, 6, 255, 4, 0 },
    { NULL, NULL, NULL, { 352, -128, 5568, 0 }, { { -240, 0, -1024, 0 }, { 1392, 0, -1024, 0 }, { -240, 0, 448, 0 }, { 1392, 0, 448, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1726, 2, 22, 255, 4, 0 },
    { NULL, NULL, NULL, { 640, -48, 6656, 0 }, { { -320, 0, -432, 0 }, { 320, 0, -432, 0 }, { -320, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 535, 0, 20, 36, 2, 0 },
    { NULL, NULL, NULL, { 2920, -64, 208, 0 }, { { -1480, 0, -160, 0 }, { 1432, 0, -160, 0 }, { -936, 0, 896, 0 }, { 984, 0, 896, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1487, 2, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 1728, -64, 576, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 4256, -64, 608, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 3936, -64, 4000, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 561, 2, 11, 255, 2, 0 },
    { NULL, NULL, NULL, { 1663, -64, 7415, 0 }, { { 669, 0, -1319, 0 }, { 650, 0, 404, 0 }, { -713, 0, -355, 0 }, { -604, 0, 1272, 0 } }, { 0, 4095, 0, 0 }, { -2896, 0, -2896, 0 }, 1476, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 4544, -64, 7104, 0 }, { { -400, 0, -512, 0 }, { 400, 0, -512, 0 }, { -400, 0, 512, 0 }, { 400, 0, 512, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 649, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, 5568, 0 }, { { -1072, 0, -576, 0 }, { 400, 0, -576, 0 }, { -1072, 0, 1184, 0 }, { 400, 0, 1184, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1593, 2, 14, 0, 4, 0 },
    { NULL, NULL, NULL, { 2976, -64, 6528, 0 }, { { -176, 0, -1024, 0 }, { 816, 0, -1024, 0 }, { -176, 0, 960, 0 }, { 816, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1305, 2, 15, 0, 132, 0 },
};

GpAreaVariant D_dryfield_night_motel_room_6_801861B4[11] = { 0 };

s32 D_dryfield_night_motel_room_6_8018620C[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

s32 D_dryfield_night_motel_room_6_80186218[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_dryfield_night_motel_room_6_80186224[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186230[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186238[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_room_6_8018620C },
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186240[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_room_6_80186218 },
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186248[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_room_6_80186224 },
};

GpRoomParamRec * D_dryfield_night_motel_room_6_80186250[8] = {
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186238,
    D_dryfield_night_motel_room_6_80186240,
    D_dryfield_night_motel_room_6_80186248,
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186230,
};

GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[16] = {
    { 3, 2, 2, 0 },
    { 3, 3, 3, 1 },
    { 3, 5, 3, 1 },
    { 3, 9, 3, 1 },
    { 3, 11, 2, 1 },
    { 3, 12, 2, 1 },
    { 3, 13, 2, 1 },
    { 3, 14, 2, 1 },
    { 3, 15, 4, 1 },
    { 3, 18, 3, 1 },
    { 3, 24, 3, 0 },
    { 3, 26, 2, 1 },
    { 3, 28, 2, 1 },
    { 3, 29, 2, 1 },
    { 3, 31, 4, 1 },
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[1] = {
    { 255, 0, 0, 0 },
};

Task * D_dryfield_night_motel_room_6_801862B4 = NULL;

RoomCutsceneRec D_dryfield_night_motel_room_6_801862B8 = { 0 };

/// "Telephone", the title of the menu panel
/// `func_dryfield_night_motel_room_6_8017EA74` runs. Two non-zero bytes follow
/// its terminator, so it stays assembly.
static const char D_dryfield_night_motel_room_6_8017D638[];

static void func_dryfield_night_motel_room_6_8017E2A4(UiList* list, UiObject* obj);
static void func_dryfield_night_motel_room_6_8017E5A0(UiList* list, UiObject* obj);
static void func_dryfield_night_motel_room_6_8017EDC8(u8* str, s32 decimals);
static u8*  func_dryfield_night_motel_room_6_8017EE38(u8* buf, s32 value, s32 decimals);
static void func_dryfield_night_motel_room_6_8017F01C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
static void func_dryfield_night_motel_room_6_8017F498(Task* task);
static void func_dryfield_night_motel_room_6_80181CD8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_motel_room_6_80182158(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws one row of the play-data statistics panel: the row label, then the
/// statistic `arg0->field_8` selects - play time, several save counters, and
/// two percentages printed with two decimals and a "%" suffix. While the row is
/// selected its help text goes to the UI holder.
void func_dryfield_night_motel_room_6_8017D6DC(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_night_motel_room_6_80182BD4,
                D_dryfield_night_motel_room_6_80182C00,
                D_dryfield_night_motel_room_6_80182C24,
                D_dryfield_night_motel_room_6_80182C54,
                D_dryfield_night_motel_room_6_80182C88,
                D_dryfield_night_motel_room_6_80182CBC,
                D_dryfield_night_motel_room_6_80182CF4,
                D_dryfield_night_motel_room_6_80182D28,
                D_dryfield_night_motel_room_6_80182D60,
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182B78);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182BA8);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_dryfield_night_motel_room_6_80182BC8);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182B80);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_dryfield_night_motel_room_6_80182BC8);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182B84);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_dryfield_night_motel_room_6_80182BC8);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182B8C);
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
            Text_Strcat(p, D_dryfield_night_motel_room_6_80182BD0);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182B98);
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
            Text_Strcat(p, D_dryfield_night_motel_room_6_80182BD0);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182BB0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_dryfield_night_motel_room_6_80182BC8);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182BB8);
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
            Text_DrawString(&req, D_dryfield_night_motel_room_6_80182BC0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// "Play Data", the title of the play-data menu panel
/// `func_dryfield_night_motel_room_6_8017EF2C` draws.
static const char D_dryfield_night_motel_room_6_8017D610[] = "Play Data";

/// Text drawn in place of a usage row's percentage once it reaches 100 percent.
static const u8 D_dryfield_night_motel_room_6_8017D61C[] = "100.0%";

/// Draws one row of a play-data usage panel from the `RoomItemUsage` block at
/// the owner task's `work`: the item's name and icon, its share of all uses as
/// a two-decimal percentage, and a gouraud bar scaled by the row's
/// `barWidths`. While the row is selected the item is previewed, and a confirm
/// opens the item's detail panel.
void func_dryfield_night_motel_room_6_8017DEA8(UiList* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_night_motel_room_6_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_dryfield_night_motel_room_6_80182BD0);
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
static void func_dryfield_night_motel_room_6_8017E2A4(UiList* list, UiObject* obj)
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
static void func_dryfield_night_motel_room_6_8017E5A0(UiList* list, UiObject* obj)
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

/// Titles of the usage panel, one per kind: weapons and Parasite Energy.
static const char D_dryfield_night_motel_room_6_8017D624[] = "Weapon Data";
static const char D_dryfield_night_motel_room_6_8017D630[] = "PE Data";

/// Task of the usage panel: draws the weapon or Parasite Energy title
/// (`spawnArg1`), and on its first tick allocates the `RoomItemUsage` /
/// `RoomPeUsage` block, spawns the panel and fills its list. Cancel closes it,
/// and a child panel that closes hands control back.
void func_dryfield_night_motel_room_6_8017E8C0(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_dryfield_night_motel_room_6_80182DC4;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_dryfield_night_motel_room_6_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, D_dryfield_night_motel_room_6_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_night_motel_room_6_80182DE8, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_dryfield_night_motel_room_6_8017E2A4(list, obj);
        } else {
            func_dryfield_night_motel_room_6_8017E5A0(list, obj);
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
static const char D_dryfield_night_motel_room_6_8017D638[12] = "Telephone\0\xDF\xDC";

/// Task of the telephone menu panel. Until the save allows it (demo scene 1
/// or a clear) it only spawns the generic panel; otherwise it lays out its
/// list and draws the title. Choosing an entry opens the item prompt, and
/// cancel closes the panel.
void func_dryfield_night_motel_room_6_8017EA74(Task* task)
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
    list          = &D_dryfield_night_motel_room_6_80182E4C;
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
        /* The literal carries its trailing "\0\1" - the room's rodata has
         * those two bytes right after the string and nothing else claims them. */
        Ui_DrawText(&(obj)->panel, D_dryfield_night_motel_room_6_8017D638);
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

/// Task of a prompt panel: on its first tick it becomes the UI holder and
/// installs the save-prompt exit callback; every tick it draws the prompt
/// lines.
void func_dryfield_night_motel_room_6_8017ED6C(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_dryfield_night_motel_room_6_8017F45C;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void func_dryfield_night_motel_room_6_8017EDC8(u8* str, s32 decimals)
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
static u8* func_dryfield_night_motel_room_6_8017EE38(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_night_motel_room_6_80182BD0);
    return buf;
}

/// Task of the play-data menu panel: draws "Play Data", and on its first tick
/// spawns the panel and lays out its list. Cancel closes it.
void func_dryfield_night_motel_room_6_8017EF2C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_night_motel_room_6_80182D9C;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_dryfield_night_motel_room_6_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_night_motel_room_6_80182DE8, 0, 0, 1, obj);
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
static void func_dryfield_night_motel_room_6_8017F01C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

/// Menu entry "Save": on a confirm while the CD is idle, opens the save panel
/// and moves the owner task to state 1.
void func_dryfield_night_motel_room_6_8017F120(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_room_6_80182B50, prompt->field_1C, 1, 0);
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

/// Menu entry "Play Data": on a confirm, opens the play-data panel and moves
/// the owner task to state 2.
void func_dryfield_night_motel_room_6_8017F204(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_room_6_80182B58, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_motel_room_6_80182E04, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Menu entry "Weapon Data": on a confirm, opens the usage panel for weapons
/// and moves the owner task to state 2.
void func_dryfield_night_motel_room_6_8017F2CC(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_room_6_80182B64, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_motel_room_6_80182E20, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Menu entry "PE Data": on a confirm, opens the usage panel for Parasite
/// Energy and moves the owner task to state 2.
void func_dryfield_night_motel_room_6_8017F394(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_room_6_80182B70, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_motel_room_6_80182E20, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
static void func_dryfield_night_motel_room_6_8017F45C(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// Sets up the room's mirror: re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block the reflection's coordinate frame and matrices live
/// in, and hangs the task off the player task so it dies with it.
/// `spawnArg1` must be 0 or 1, and 0 also raises `GameSession::field_4E`. The
/// two child tasks reflect the player's held-object tasks
/// (`GameActor::field_920` / `field_924`).
static void func_dryfield_night_motel_room_6_8017F498(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GpCoord*        parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = gameGetPtrSlot(3);
    if (Gp_AttachTmd(task, owner->extra.tmd->source) == NULL) {
        taskKill(task);
        return;
    }
    extra = task->extra.tmd;
    parts = extra->coords;
    if ((u32)task->spawnArg1.value >= 2U) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    extra->tpage = 6;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    extra->flags    = 0x10;
    extra->otOffset = 0x1F;
    if (task->spawnArg1.value == 0) {
        gGameSession->field_4E = 1;
    }
    parts->sub      = &work->coord;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    Task_Reparent(owner, task);
    task->state++;
    work->viewFlg   = gGfxViewCoord.flg & 0x7FFFFFFF;
    work->field_4   = 1;
    work->configRev = -1;
    extra->flags   |= 0x80;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->work;
    for (i = 0; i < 2; i++) {
        child = (&actor->field_920)[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E74, 1, i, task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_dryfield_night_motel_room_6_8017F64C(task);
}

/// Per-frame update of the room's mirror, the task
/// `func_dryfield_night_motel_room_6_8017F498` sets up.
///
/// When the player's equipped weapon changes it spawns reflection tasks for
/// the player's two held-object tasks. When the view moves it rebuilds the
/// reflection's coordinate frame. Mirror 0 copies the view matrix with its
/// second row negated and applies location-specific corrections. The other
/// mirror reflects through a plane chosen by the current stage, area and view. On the frame after mirror 0
/// rebuilds, it queues packets that copy the frame buffer into the off-screen
/// strip at x = `width`. In stages 1 and 5 it projects the reflected body to
/// find its screen rectangle and, where that overlaps the mirror's clip
/// rectangle, draws quads sampling that strip. Otherwise the reflection is
/// hidden. Every frame it copies the player's pose and light matrices onto the
/// reflection.
static void func_dryfield_night_motel_room_6_8017F64C(Task* task)
{
    RoomMirrorWork*          work;
    PlayerStatus*            status;
    TmdObject*               extra;
    TmdObject*               model;
    Task*                    owner;
    GameActor*               actor;
    Task*                    child;
    Task*                    spawned;
    RoomMirrorPlaneScratch*  plane;
    RoomMirrorExtentScratch* extent;
    GpCoord*                 parts;
    GpCoord*                 refPart;
    DR_AREA*                 drArea;
    DR_STP*                  drStp;
    DR_OFFSET*               drOffset;
    SPRT*                    sprt;
    DR_TPAGE*                tpage;
    TILE*                    tile;
    POLY_FT4*                poly;
    s32                      stage;
    s32                      area;
    s32                      view;
    s32                      width;
    s32                      viewFlg;
    s32                      copyPending;
    s32                      halfWidth;
    s32                      texX;
    s32                      i;
    s32                      layer;
    u32                      j;

    width  = 0x1C0;
    work   = task->work;
    extra  = task->extra.tmd;
    stage  = Mc_SaveData[0].state.at4.loc.stage;
    area   = Mc_SaveData[0].state.at4.loc.area;
    view   = Mc_SaveData[0].state.at4.loc.view;
    status = &Player_Status;
    if (stage == 5) {
        width = 0x140;
    }
    if (work->configRev != status->weapon) {
        actor           = gameGetPtrSlot(3)->work;
        work->configRev = status->weapon;
        for (i = 0; i < 2; i++) {
            child = (&actor->field_918)[i];
            if (child != NULL) {
                spawned = Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E74, 1, i + 2, task);
                if (spawned != NULL) {
                    Task_Reparent(child, spawned);
                }
            }
        }
    }
    extra->flags |= 0x10;
    viewFlg       = gGfxViewCoord.flg & 0x7FFFFFFF;
    if (work->viewFlg != viewFlg) {
        GpCoord* sub;

        work->viewFlg     = viewFlg;
        sub               = gGfxViewCoord.sub;
        work->field_A0[0] = -0xA0;
        work->field_A0[1] = 0xA0;
        work->coord.flg   = 0;
        work->field_A0[2] = -0x78;
        work->field_A0[3] = 0x78;
        plane             = (RoomMirrorPlaneScratch*)SCRATCH_PUSH_BYTES(0x70);
        work->coord.sub   = sub;
        if (task->spawnArg1.value == 0) {
            work->field_4     = 1;
            work->coord.coord = gGfxViewCoord.coord;
            plane->viewRow.vx = work->coord.coord.m[1][0];
            plane->viewRow.vy = work->coord.coord.m[1][1];
            plane->viewRow.vz = work->coord.coord.m[1][2];
            gte_lddp(-0x1000);
            gte_ldsv(&plane->viewRow);
            gte_gpf12();
            gte_stsv(&plane->viewRow);
            work->coord.coord.m[1][0] = plane->viewRow.vx;
            work->coord.coord.m[1][1] = plane->viewRow.vy;
            work->coord.coord.m[1][2] = plane->viewRow.vz;
            if (stage == 5) {
                if (area == 7) {
                    if (view >= 6 && view < 12 && gGameSession->at4.loc.room == 2) {
                        work->coord.coord.t[1] += 0x9B;
                        extra->flags           &= ~0x80;
                        work->field_8           = 0;
                    } else {
                        work->field_4 = 0;
                        extra->flags |= 0x80;
                    }
                }
            } else if (area == 1) {
                extra->flags |= 0x80;
                if (view == 9) {
                    work->field_4 = 0;
                }
            } else {
                if (area != 0x11) {
                    work->coord.coord.t[1] += 0x69;
                }
                work->field_8 = 1;
                if ((area == 0x11 && view == 5) || (area == 2 && (view == 7 || view == 5))) {
                    extra->flags |= 0x80;
                } else {
                    extra->flags &= ~0x80;
                }
            }
        } else {
            model         = task->extra.tmd;
            model->flags &= ~0x80;
            if (stage == 1) {
                switch (area) {
                    case 0x11:
                        switch (view) {
                            case 2:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = -0x1518;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 3:
                                plane->normal.vx = 0x64;
                                plane->normal.vy = 0;
                                plane->normal.vz = -0x384;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = -0x640;
                                break;
                            case 4:
                                work->field_A0[0] = -0x14;
                                work->field_A0[1] = 0x14;
                                plane->normal.vx  = -0x1000;
                                plane->normal.vy  = 0;
                                plane->normal.vz  = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x1644;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    case 1:
                        switch (view) {
                            case 6:
                                work->field_A0[1] = 0x64;
                                work->field_A0[0] = 0;
                                plane->normal.vx  = -0x1000;
                                plane->normal.vy  = 0;
                                plane->normal.vz  = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x1AF4;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                model->otOffset  = 0x1F;
                                break;
                            case 7:
                            case 8:
                                plane->normal.vx = 0;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0x1000;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0x14B4;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    case 2:
                        switch (view) {
                            case 2:
                            case 5:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x170C;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 4:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = -0x1644;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 3:
                                plane->normal.vx = -0x64;
                                plane->normal.vy = 0;
                                plane->normal.vz = -0x384;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = -0x640;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    default:
                        model->flags |= 0x80;
                        break;
                }
            } else if (view == 8 || view == 1) {
                plane->normal.vx = -0x1000;
                plane->normal.vy = 0;
                plane->normal.vz = 0;
                VectorNormalSS(&plane->normal, &plane->normal);
                plane->offset.vx = 0xA38;
                plane->offset.vy = 0;
                plane->offset.vz = 0;
            } else {
                model->flags |= 0x80;
            }
            if (!(model->flags & 0x80)) {
                plane->leastAbs = plane->normal.vx;
                if (plane->leastAbs < 0) {
                    plane->leastAbs = -plane->leastAbs;
                }
                plane->leastAxis = 0;
                plane->axisAbs   = plane->normal.vy;
                if (plane->axisAbs < 0) {
                    plane->axisAbs = -plane->axisAbs;
                }
                if (plane->leastAbs > plane->axisAbs) {
                    plane->leastAbs  = plane->axisAbs;
                    plane->leastAxis = 1;
                }
                plane->axisAbs = plane->normal.vz;
                if (plane->axisAbs < 0) {
                    plane->axisAbs = -plane->axisAbs;
                }
                if (plane->leastAbs > plane->axisAbs) {
                    plane->leastAbs  = plane->axisAbs;
                    plane->leastAxis = 2;
                }
                plane->refAxis.vx = 0;
                if (plane->leastAxis == 0) {
                    plane->refAxis.vx = 0x1000;
                }
                plane->refAxis.vy = 0;
                if (plane->leastAxis == 1) {
                    plane->refAxis.vy = 0x1000;
                }
                plane->refAxis.vz = 0;
                if (plane->leastAxis == 2) {
                    plane->refAxis.vz = 0x1000;
                }
                Gfx_OrthonormalBasis(&plane->basis, &plane->normal, &plane->refAxis);
                gte_TransposeMatrix(&plane->basis, &plane->reflect);
                plane->reflect.m[2][0] = -plane->reflect.m[2][0];
                plane->reflect.m[2][1] = -plane->reflect.m[2][1];
                plane->reflect.m[2][2] = -plane->reflect.m[2][2];
                gte_MulMatrix0(&plane->basis, &plane->reflect, &plane->reflect);
                work->coord.coord      = plane->reflect;
                work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + plane->offset.vx;
                work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + plane->offset.vy;
                work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + plane->offset.vz;
                gfxRotateSv(&plane->reflect, &plane->offset);
                work->coord.coord.t[0] -= plane->offset.vx;
                work->coord.coord.t[1] -= plane->offset.vy;
                work->coord.coord.t[2] -= plane->offset.vz;
                work->field_8           = 1;
            }
        }
        work->field_C = extra->flags;
        SCRATCH_POP_BYTES(0x70);
    }

    copyPending = work->field_4;
    if (copyPending == 1 && task->spawnArg1.value == 0 && !(area == 1 && view == 0xF) && gDisplayState.pendingMode == 0) {
        u16  ofs[2];
        RECT rect;

        work->field_4   = 0;
        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = 0;
        rect.y          = gDisplayState.drawBuffer * 0x110;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        drStp           = (DR_STP*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_STP);
        SetDrawStp(drStp, 0);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset        = (DR_OFFSET*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_OFFSET);
        ofs[0]          = 0xA0;
        ofs[1]          = gDisplayState.drawBuffer * 0x110 + 0x78;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        sprt            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(SPRT);
        sprt->x0        = -0xA0;
        sprt->y0        = -0x78;
        sprt->w         = 0xA0;
        sprt->h         = 0xF0;
        sprt->u0        = 0;
        sprt->v0        = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage           = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        sprt            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(SPRT);
        sprt->x0        = 0;
        sprt->y0        = -0x78;
        sprt->w         = 0xA0;
        sprt->h         = 0xF0;
        sprt->u0        = 0x20;
        sprt->v0        = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage           = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        tile            = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(TILE);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->r0 = tile->g0 = 2;
        tile->b0            = 2;
        tile->w             = 0x140;
        tile->h             = 0xF0;
        addPrim(&gGpuCurrentOt[0x3FF], tile);

        drStp           = (DR_STP*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_STP);
        SetDrawStp(drStp, 1);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset        = (DR_OFFSET*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_OFFSET);
        ofs[0]          = width + 0xA0;
        ofs[1]          = 0x178;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = width;
        rect.y          = 0x100;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);
    }

    extra->flags = work->field_C;
    if (!(extra->flags & 0x80) && gGameSession->field_65 == 0) {
        parts   = task->extra.tmd->coords;
        owner   = gameGetPtrSlot(3);
        refPart = &parts[1];
        if (owner != NULL) {
            TmdObject* src       = owner->extra.tmd;
            GpCoord*   srcCoords = src->coords;

            parts->flg = 0;
            j          = 0;
            if (src->partCount != 0) {
                GpCoord* from = (GpCoord*)&srcCoords->coord;
                GpCoord* to   = (GpCoord*)&parts->coord;

                do {
                    *(MATRIX*)to = *(MATRIX*)from;
                    to++;
                    from++;
                } while (++j < src->partCount);
            }
        }
        if (stage == 1 || stage == 5) {
            extent = (RoomMirrorExtentScratch*)SCRATCH_PUSH_BYTES(0x34);
            if (gGameSession->eventState != 0) {
                Gp_UpdateCoord(refPart);
                gte_SetTransMatrix(&refPart->workm);
                gte_SetRotMatrix(&refPart->workm);
                extent->pos.vx = 0;
                extent->pos.vy = -0x3E8;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyHead, &extent->dp, &extent->flag, &extent->otzHead);
                extent->pos.vx = 0;
                extent->pos.vy = 0x3E8;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyFoot, &extent->dp, &extent->flag, &extent->otzFoot);
            } else {
                Gp_UpdateCoord(parts);
                gte_SetTransMatrix(&parts->workm);
                gte_SetRotMatrix(&parts->workm);
                extent->pos.vx = 0;
                extent->pos.vy = -0x7D0;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyHead, &extent->dp, &extent->flag, &extent->otzHead);
                extent->pos.vx = 0;
                extent->pos.vy = 0;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyFoot, &extent->dp, &extent->flag, &extent->otzFoot);
            }
            if (extent->sxyFoot.vy > extent->sxyHead.vy) {
                extent->sxyHead.vx = extent->sxyFoot.vy;
                extent->sxyFoot.vy = extent->sxyHead.vy;
                extent->sxyHead.vy = extent->sxyHead.vx;
            }
            extent->sxyFoot.vy -= 0x10;
            extent->sxyHead.vy += 0x10;
            halfWidth           = (extent->sxyHead.vy - extent->sxyFoot.vy) >> 1;
            if (halfWidth >= 0x60) {
                halfWidth = 0x5F;
            }
            if ((GP_LOC_WORD(Mc_SaveData[0].state.at4.loc) & GP_LOC_AREA_VIEW) == GP_LOC_KEY(0, 2, 0, 5)) {
                if (task->spawnArg1.value == 0) {
                    halfWidth = 0x5F;
                } else {
                    extent->otzFoot = extent->otzHead + 0xA;
                }
            }
            extent->left = extent->sxyFoot.vx - halfWidth;
            if (extent->left < -0xA0) {
                extent->left = -0xA0;
            }
            extent->right = extent->sxyFoot.vx + halfWidth;
            if (extent->right > 0xA0) {
                extent->right = 0xA0;
            }
            extent->top = extent->sxyFoot.vy;
            if (extent->top < -0x78) {
                extent->top = -0x78;
            }
            extent->bottom = extent->sxyHead.vy;
            if (extent->bottom > 0x78) {
                extent->bottom = 0x78;
            }
            if (extent->top < work->field_A0[3] && work->field_A0[2] < extent->bottom && extent->left < work->field_A0[1] &&
                work->field_A0[0] < extent->right) {
                DR_TPAGE* mode;

                mode            = (DR_TPAGE*)gGpuPrimCursor;
                texX            = extent->left + (u16)(width + 0xA0);
                extent->texX    = texX & 0xFFC0;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
                for (layer = work->field_8; layer < 3; layer++) {
                    poly            = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor += sizeof(POLY_FT4);
                    setPolyFT4(poly);
                    setSemiTrans(poly, 1);
                    if (work->field_8 == 1) {
                        setShadeTex(poly, 0);
                        poly->r0 = poly->g0 = poly->b0 = 0x80;
                    } else {
                        setShadeTex(poly, 1);
                    }
                    poly->x0 = poly->x2 = extent->left;
                    poly->x1 = poly->x3 = extent->right;
                    poly->y0 = poly->y1 = extent->top;
                    poly->y2 = poly->y3 = extent->bottom;
                    poly->tpage         = getTPage(2, layer, extent->texX, 0x100);
                    poly->u0 = poly->u2 = poly->x0 + 0xA0 + width - extent->texX;
                    poly->u1 = poly->u3 = poly->x1 + 0xA0 + width - extent->texX;
                    poly->v0 = poly->v1 = poly->y0 + 0x78;
                    poly->v2 = poly->v3 = poly->y2 + 0x78;
                    addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                            poly);
                }
                mode            = (DR_TPAGE*)gGpuPrimCursor;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
            } else {
                extra->flags |= 0x80;
            }
            SCRATCH_POP_BYTES(0x34);
        }
    }

    {
        GpCoord*   ownerParts;
        TmdObject* ownerBody;
        GpCoord*   ownParts;
        MATRIX     mtx;

        ownerParts  = gameGetPtrSlot(3)->extra.tmd->coords;
        ownerBody   = gameGetPtrSlot(3)->extra.tmd;
        ownParts    = task->extra.tmd->coords;
        work->light = *ownerBody->lightMtx;
        work->color = *ownerBody->colorMtx;
        Gp_UpdateCoord(ownParts);
        gte_TransposeMatrix(&ownParts->workm, &mtx);
        gte_MulMatrix0(&ownerParts->workm, &mtx, &mtx);
        gte_MulMatrix0(&work->light, &mtx, &work->light);
    }
}

/// Scale applied to reflections with `spawnArg1 >= 2`: X negated, Y and Z kept.
static const VECTOR D_dryfield_night_motel_room_6_8017D644 = { -0x1000, 0x1000, 0x1000 };

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task the room set up and the parent is the held-object task being reflected. On the first frame it
/// clones the parent's TMD source, parents the clone's root coordinate to the
/// mirrored player's corresponding part, points the clone at the mirror's
/// light and color matrices and negates the X translation; every frame it
/// republishes the mirror model's draw flags onto the clone.
void func_dryfield_night_motel_room_6_80180FD0(Task* task)
{
    Task*           mirror;
    TmdObject*      mirrorExtra;
    RoomMirrorWork* work;
    GpCoord*        mirrorPart;
    TmdObject*      src;
    GpCoord*        srcParts;
    TmdObject*      extra;
    GpCoord*        parts;
    VECTOR          scale;
    u16             flags;

    if (task->parent == NULL) {
        Task_CallExit(task);
    }
    mirror      = (Task*)task->spawnArg2.pointer;
    mirrorPart  = &mirror->extra.tmd->coords[D_dryfield_night_motel_room_6_80182E70[task->spawnArg1.value]];
    work        = (RoomMirrorWork*)mirror->work;
    mirrorExtra = mirror->extra.tmd;
    if (task->state == 0) {
        src      = task->parent->extra.tmd;
        srcParts = src->coords;
        if (Gp_AttachTmd(task, src->source) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra        = task->extra.tmd;
        parts        = extra->coords;
        extra->tpage = src->tpage;
        tmdProcessStream(extra);
        tmdProcessStream(extra);
        extra->flags    = 0x10;
        extra->otOffset = 0x1F;
        parts->sub      = mirrorPart;
        extra->lightMtx = &work->light;
        extra->colorMtx = &work->color;
        if (task->spawnArg1.value >= 2) {
            scale = D_dryfield_night_motel_room_6_8017D644;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0] = -srcParts->coord.t[0];
        parts->coord.t[1] = srcParts->coord.t[1];
        parts->coord.t[2] = srcParts->coord.t[2];
        parts->flg        = 0;
        task->state++;
    }
    extra        = task->extra.tmd;
    flags        = mirrorExtra->flags;
    extra->flags = flags;
    if (task->spawnArg1.value >= 2) {
        extra->flags = flags & 0xFFEF;
    }
}

/// Mirror task: runs the set-up state, then the per-frame state.
void func_dryfield_night_motel_room_6_801811A0(Task* task)
{
    TaskFunc states[2] = {
        func_dryfield_night_motel_room_6_8017F498,
        func_dryfield_night_motel_room_6_8017F64C,
    };

    states[task->state](task);
}

/// The room's cutscene task, driven by the script record at `spawnArg2`. It
/// hides the HUD and holds the player's (and any ally's) weapon, forces the
/// script's area id and loads its cap file, then starts the scene with its
/// sound task and waits for it to end or for the player to skip it. On the way
/// out it advances the story flags the scene belongs to, runs the follow-up
/// cap commands, and restores the view, weapons and HUD before killing itself.
void func_dryfield_night_motel_room_6_801811F0(Task* task)
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
            D_dryfield_night_motel_room_6_801862B4 = NULL;
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
            D_dryfield_night_motel_room_6_801862B4 =
                Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E8C, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_dryfield_night_motel_room_6_801862B4);
                task->state++;
            } else if (Task_PollKill(D_dryfield_night_motel_room_6_801862B4, &poll) != 0) {
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

/// State handlers of the room entry task `func_dryfield_night_motel_room_6_80181C80`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_motel_room_6_8017D6B4 = {
    {
        func_dryfield_night_motel_room_6_80181C34,
        func_dryfield_night_motel_room_6_80181C78,
        taskKill,
    },
};

/// Handler of message 0x13F0 in the room's message table. For event 0x16 it
/// fills in the cutscene script record - the cap file and fade chosen from
/// flag nibble 0x7A and the stage, and the scene's sound events - and spawns
/// the cutscene task on it. Any other event goes to
/// `func_dryfield_night_motel_room_6_80181A9C`.
s32 func_dryfield_night_motel_room_6_8018175C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 count;

    count = 0;
    if (arg2 == 0x16) {
        D_dryfield_night_motel_room_6_801862B8.field_0 = 0xC;
        D_dryfield_night_motel_room_6_801862B8.field_1 = 1;
        switch (GameFlag_GetNibble(0x7A)) {
            case 0 ... 3:
                if (gGameSession->at4.loc.stage == 2) {
                    count                                           = 4;
                    D_dryfield_night_motel_room_6_801862B8.field_14 = 0x3C0;
                    D_dryfield_night_motel_room_6_801862B8.field_3  = 1;
                } else {
                    count                                           = 2;
                    D_dryfield_night_motel_room_6_801862B8.field_14 = 0x380;
                    D_dryfield_night_motel_room_6_801862B8.field_3  = 1;
                }
                break;
            case 4 ... 6:
                count                                           = 2;
                D_dryfield_night_motel_room_6_801862B8.field_14 = 0x3C0;
                D_dryfield_night_motel_room_6_801862B8.field_3  = count;
                break;
        }
        D_dryfield_night_motel_room_6_801862B8.field_2  = 0;
        D_dryfield_night_motel_room_6_801862B8.field_4  = Gp_PackStageSndId(0x521E0008);
        D_dryfield_night_motel_room_6_801862B8.field_8  = Gp_PackStageSndId(0x521E000B);
        D_dryfield_night_motel_room_6_801862B8.field_10 = Gp_PackStageSndId(0x521E0009);
        D_dryfield_night_motel_room_6_801862B8.field_C  = Gp_PackStageSndId(0x521E000A);
        Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E8C, 0, count, &D_dryfield_night_motel_room_6_801862B8);
    } else {
        func_dryfield_night_motel_room_6_80181A9C(arg0, arg1, arg2, arg3);
    }
    return 0;
}

/// The room's story task: holds the player's weapon and runs cap command 0x10.
/// If the scene then reports event key 0xB the task ends there, giving the
/// weapon back. Otherwise it sets flag nibble 0x70 to 2 and, once the scene is
/// over, refills the player's HP and MP, stops the sound, applies the story's
/// area records (the second list only while nibble 0xCE is set), updates the
/// story flags, moves the saved location to area 8, warp 1, room 1 and spawns
/// task 0x11.
void func_dryfield_night_motel_room_6_8018189C(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto advance;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    Gp_MsgPlayerWeapon(0);
    Gp_RunCapCmd1(0x10);
    goto advance;

L_case2:
    if (Gp_GetCapEventKey() == 0xB) {
        taskKill(task);
        Gp_MsgPlayerWeapon(1);
    }
    goto advance;

L_case3:
    GameFlag_SetNibble(0x70, 2);
    goto advance;

L_case4:
    if (Gp_CapBusy() != 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case5:
    Gp_FillPlayerHpMp();
    SndEvt_EnqueueType7(0x80000000, 0);
    Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_80186270);
    if (GameFlag_GetNibble(0xCE) != 0) {
        Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_801862B0);
    }
    GameFlag_SetNibble(0x59, 1);
    GameFlag_SetNibble(0x5A, 2);
    GameFlag_SetNibble(0x30, 0);
    Mc_SaveData[0].state.at4.loc.area = 8;
    Mc_SaveData[0].state.at4.loc.warp = 1;
    Mc_SaveData[0].state.at4.loc.room = 1;
    gDisplayState.roomVariant   = 1;
    Task_Spawn(0, 0x11, 0, 0);
    taskKill(task);
}

/// Sound task: plays the sound event `spawnArg2` on its first tick and again
/// at tick 0x50, then kills itself at tick 0x78.
void func_dryfield_night_motel_room_6_80181A0C(Task* task)
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

/// Runs the cap command for events 6, 0xD and 0xB, picking an alternative
/// command while flag nibble 0x61 is set. Event 6 instead spawns the story
/// task once nibble 0x6C is positive and nibble 0x70 is below 2.
static s32 func_dryfield_night_motel_room_6_80181A9C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x14);
        } else if (GameFlag_GetNibble(0x6C) > 0 && GameFlag_GetNibble(0x70) < 2) {
            Task_SpawnFromTable(&D_dryfield_night_motel_room_6_80182EE0, 0, 0x11, 0);
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xD) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x13);
        } else {
            Gp_RunCapCmd1(0xD);
        }
    }
    if (arg2 == 0xB) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x15);
        } else {
            Gp_RunCapCmd1(0xB);
        }
    }
    return 0;
}

/// Handler of message 0x13F1 in the room's message table: does nothing.
s32 func_dryfield_night_motel_room_6_80181B74(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler of message 0x13EE in the room's message table: copies the incoming record onto the outgoing one and,
/// for message 0x1D with `field_5` clear, answers 1 or 3 in `field_3`
/// depending on whether flag nibble 0x61 is set. Always returns 1.
s32 func_dryfield_night_motel_room_6_80181B7C(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    s32 nib;

    *out = *in;
    if (in->prefix.packed == 0x1D && in->field_5 == 0) {
        nib = GameFlag_GetNibble(0x61);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->field_3 = nib;
    }
    return 1;
}

/// Handler of message 0x13EF in the room's message table: does nothing.
s32 func_dryfield_night_motel_room_6_80181BF8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler of message 0x13F2 in the room's message table: plays sound event
/// 0x531E000C for event 0x63.
s32 func_dryfield_night_motel_room_6_80181C00(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x531E000C, 0, 0);
    }
    return 0;
}

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
static void func_dryfield_night_motel_room_6_80181C34(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_6_80182EB0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room entry task: nothing left to do but idle.
static void func_dryfield_night_motel_room_6_80181C78(Task* task)
{
}

/// Room entry task: runs the state handler `D_dryfield_night_motel_room_6_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_night_motel_room_6_80181C80(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_6_8017D6B4;
    sp.funcs[task->state](task);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 48 / otz`. `arg1` scales `gDisplayState.animFrame`
/// into `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
static void func_dryfield_night_motel_room_6_80181CD8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        block   = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 48) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
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
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
static void func_dryfield_night_motel_room_6_80182158(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
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
    SCRATCH_POP_BYTES(0x14);
}

/// Draws the room's highlight for the current camera view: the diamond marker
/// in views 3 and 4, the glow disc in view 12, nothing otherwise.
void func_dryfield_night_motel_room_6_80182AE0(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 3:
        case 4:
            func_dryfield_night_motel_room_6_80181CD8(&D_dryfield_night_motel_room_6_80182EF8[0], 0x60, 0x60);
            break;
        case 12:
            func_dryfield_night_motel_room_6_80182158(&D_dryfield_night_motel_room_6_80182EF8[0], 0x60, 0x80);
            break;
    }
}
