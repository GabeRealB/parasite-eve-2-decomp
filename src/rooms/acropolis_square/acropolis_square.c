#include "rooms/acropolis_square.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
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

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern UiObjectDesc D_800611E4;

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
extern u8 D_acropolis_square_80183464[];

/// Task descriptor of the held-object reflections the mirror spawns.
extern TaskDesc D_acropolis_square_80183468[];

/// Labels of the four menu entries of the play-data menu panel: "Save",
/// "Play Data", "Weapon Data" and "PE Data".
extern u8 D_acropolis_square_80183480[];
extern u8 D_acropolis_square_80183488[];
extern u8 D_acropolis_square_80183494[];
extern u8 D_acropolis_square_801834A0[];

/// Row labels of the play-data statistics panel, one per row
/// `func_acropolis_square_8017F46C` draws.
extern u8 D_acropolis_square_801834A8[];
extern u8 D_acropolis_square_801834D8[];
extern u8 D_acropolis_square_801834B0[];
extern u8 D_acropolis_square_801834B4[];
extern u8 D_acropolis_square_801834BC[];
extern u8 D_acropolis_square_801834C8[];
extern u8 D_acropolis_square_801834E0[];
extern u8 D_acropolis_square_801834E8[];
extern u8 D_acropolis_square_801834F0[];

/// The suffix appended to that panel's count rows.
extern u8 D_acropolis_square_801834F8[];

/// The "%" suffix the room's percentage formatters append.
extern u8 D_acropolis_square_80183500[];

/// Help texts of the statistics panel's nine rows, handed to the UI holder for
/// the selected row.
extern u8 D_acropolis_square_80183504[];
extern u8 D_acropolis_square_80183530[];
extern u8 D_acropolis_square_80183554[];
extern u8 D_acropolis_square_80183584[];
extern u8 D_acropolis_square_801835B8[];
extern u8 D_acropolis_square_801835EC[];
extern u8 D_acropolis_square_80183624[];
extern u8 D_acropolis_square_80183658[];
extern u8 D_acropolis_square_80183690[];

/// The play-data menu panel's list.
extern UiList D_acropolis_square_801836CC;

/// The usage panel's list.
extern UiList D_acropolis_square_801836F4;

/// UI descriptor the play-data panels spawn when they first open.
extern UiObjectDesc D_acropolis_square_80183718;

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_acropolis_square_80183734;
extern UiObjectDesc D_acropolis_square_80183750;

/// The telephone menu panel's list.
extern UiList D_acropolis_square_8018377C;

/// Task descriptor table of the room's cutscenes: entry 0 is the cutscene
/// runner, spawned with a cutscene record as its argument, and entry 1 the
/// sound task the runner spawns for the scene.
extern TaskDesc D_acropolis_square_801837A0[];

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_acropolis_square_801837C4[];

extern TaskDesc D_acropolis_square_80183808[];
extern s32      D_acropolis_square_8018382C;
extern s32      D_acropolis_square_80183830;
extern GpEvsCmd D_acropolis_square_80183834[];
extern GpEvsCmd D_acropolis_square_8018399C[];
extern GpEvsCmd D_acropolis_square_801838DC[];
extern GpEvsCmd D_acropolis_square_80183A5C[];
extern s32      D_acropolis_square_80183B34[];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(s32, s32, s32);
    } handler;
} AcropolisSquareMessageEntry;
STATIC_ASSERT_SIZEOF(AcropolisSquareMessageEntry, 8);

extern AcropolisSquareMessageEntry D_acropolis_square_80183B58[2];
extern s16                         D_acropolis_square_80183B68[];
extern s32                         D_acropolis_square_80183B98;

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.

extern s32   D_acropolis_square_80188898;
extern Task* D_acropolis_square_8018889C;
extern s32   D_acropolis_square_801888A0;
extern s32   D_acropolis_square_801888A4;

/// The scene sub-task while it runs, NULL otherwise.
extern Task* D_acropolis_square_801888A8;

/// The cutscene record the room hands entry 0 of `D_acropolis_square_801837A0`.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomCutsceneRec value;
    u8              retained[8];
} AcropolisSquareStorage88AC;
STATIC_ASSERT_SIZEOF(AcropolisSquareStorage88AC, 32);

extern AcropolisSquareStorage88AC D_acropolis_square_801888AC;

extern GpCoord D_acropolis_square_801888CC;

static void func_acropolis_square_8017D8C8(Task* task);
static void func_acropolis_square_801811EC(Task* task);
static void func_acropolis_square_80182260(Task* task);
static void func_acropolis_square_801822A4(Task* task);

void func_acropolis_square_8017F46C(UiList*, UiObject*);
void func_acropolis_square_8017FC38(UiList*, UiObject*);
void func_acropolis_square_80180650(Task*);
void func_acropolis_square_80180AFC(Task*);
void func_acropolis_square_80180CBC(Task*);
void func_acropolis_square_80180EB0(UiList*, UiObject*);
void func_acropolis_square_80180F94(UiList*, UiObject*);
void func_acropolis_square_8018105C(UiList*, UiObject*);
void func_acropolis_square_80181124(UiList*, UiObject*);
void func_acropolis_square_80181228(Task*);

void func_acropolis_square_8017F24C(Task*);

void func_acropolis_square_80181228(Task*);
void func_acropolis_square_80182048(Task*);

s32  func_acropolis_square_80181794(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_acropolis_square_801819BC(Task*, s32, s32, s32);
s32  func_acropolis_square_801820D8(Task*, s32, GpMsg13EF*, GpMessageArg);
s32  func_acropolis_square_80182108(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_acropolis_square_80182110(Task*, s32, s32, GpMessageArg);
void func_acropolis_square_80181AEC(Task*);
void func_acropolis_square_80181DD0(Task*);
void func_acropolis_square_80182148(Task*);
void func_acropolis_square_80182200(s32);

extern GpGridParams   D_acropolis_square_8018519C[1];
extern GpObj4C        D_acropolis_square_801851C0[16];
extern GpObj4C        D_acropolis_square_80185680[26];
extern GpRoomBoundVec D_acropolis_square_80186480[16];
extern GpRoomCoordSet D_acropolis_square_80186468[1];

u8 D_acropolis_square_80183464[4] = {
    12,
    8,
    12,
    8,
};

TaskDesc D_acropolis_square_80183468[2] = {
    { 0, 112, func_acropolis_square_8017F41C, { .model = NULL } },
    { 0, 112, func_acropolis_square_8017F24C, { .model = NULL } },
};

u8 D_acropolis_square_80183480[8] = {
    83, 97, 118, 101, 0, 0, 0, 0,
};

u8 D_acropolis_square_80183488[12] = {
    80, 108, 97, 121, 32, 68, 97, 116, 97, 0, 0, 0,
};

u8 D_acropolis_square_80183494[12] = {
    87, 101, 97, 112, 111, 110, 32, 68, 97, 116, 97, 0,
};

u8 D_acropolis_square_801834A0[8] = {
    80, 69, 32, 68, 97, 116, 97, 0,
};

u8 D_acropolis_square_801834A8[8] = {
    84, 105, 109, 101, 0, 0, 0, 0,
};

u8 D_acropolis_square_801834B0[4] = {
    87, 111, 110, 0,
};

u8 D_acropolis_square_801834B4[8] = {
    69, 115, 99, 97, 112, 101, 100, 0,
};

u8 D_acropolis_square_801834BC[12] = {
    66, 97, 116, 116, 108, 101, 115, 32, 119, 111, 110, 0,
};

u8 D_acropolis_square_801834C8[16] = {
    69, 120, 116, 101, 114, 109, 105, 110, 97, 116, 101, 100, 0, 0, 0, 0,
};

u8 D_acropolis_square_801834D8[8] = {
    83, 97, 118, 101, 100, 0, 0, 0,
};

u8 D_acropolis_square_801834E0[8] = {
    67, 108, 101, 97, 114, 101, 100, 0,
};

u8 D_acropolis_square_801834E8[8] = {
    77, 97, 120, 32, 69, 88, 80, 0,
};

u8 D_acropolis_square_801834F0[8] = {
    77, 97, 120, 32, 66, 80, 0, 0,
};

u8 D_acropolis_square_801834F8[8] = {
    32, 116, 105, 109, 101, 115, 0, 0,
};

u8 D_acropolis_square_80183500[4] = {
    37, 0, 0, 0,
};

u8 D_acropolis_square_80183504[44] = {
    84, 111, 116, 97, 108, 32, 97, 109, 111, 117, 110, 116, 32, 111, 102, 10,
    116, 105, 109, 101, 32, 115, 112, 101, 110, 116, 32, 102, 111, 114, 32, 116,
    104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0, 0,
};

u8 D_acropolis_square_80183530[36] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 115, 97, 118, 101, 115, 10,
    117, 115, 101, 100, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_acropolis_square_80183554[48] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 110, 101, 109, 105, 101, 115, 10, 100, 101, 102, 101, 97, 116, 101, 100,
    32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0,
};

u8 D_acropolis_square_80183584[52] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 115, 99, 97, 112, 101, 115, 10, 102, 114, 111, 109, 32, 98, 97, 116,
    116, 108, 101, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101,
    46, 0, 0, 0,
};

u8 D_acropolis_square_801835B8[52] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 98, 97, 116, 116, 108, 101, 115,
    32, 119, 111, 110, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_acropolis_square_801835EC[56] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 101, 110, 101, 109, 105, 101, 115,
    32, 100, 101, 102, 101, 97, 116, 101, 100, 32, 105, 110, 32, 116, 104, 105,
    115, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_acropolis_square_80183624[52] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 116, 105, 109, 101, 115, 32,
    121, 111, 117, 32, 104, 97, 118, 101, 10, 99, 108, 101, 97, 114, 101, 100,
    32, 116, 104, 101, 32, 103, 97, 109, 101, 32, 115, 111, 32, 102, 97, 114,
    46, 0, 0, 0,
};

u8 D_acropolis_square_80183658[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 69, 88, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10,
    98, 121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104,
    101, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_acropolis_square_80183690[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 66, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10, 98,
    121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104, 101,
    32, 103, 97, 109, 101, 46, 0, 0,
};

UiListItemFunc D_acropolis_square_801836C8[1] = {
    func_acropolis_square_8017F46C,
};

UiList D_acropolis_square_801836CC = { D_acropolis_square_801836C8, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_acropolis_square_801836F0[1] = {
    func_acropolis_square_8017FC38,
};

UiList D_acropolis_square_801836F4 = { D_acropolis_square_801836F0, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_acropolis_square_80183718 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_acropolis_square_80180AFC, 0 };

UiObjectDesc D_acropolis_square_80183734 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_acropolis_square_80180CBC, 0 };

UiObjectDesc D_acropolis_square_80183750 = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_acropolis_square_80180650, 0 };

UiListItemFunc D_acropolis_square_8018376C[4] = {
    func_acropolis_square_80180EB0,
    func_acropolis_square_80180F94,
    func_acropolis_square_8018105C,
    func_acropolis_square_80181124,
};

UiList D_acropolis_square_8018377C = { D_acropolis_square_8018376C, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

void func_acropolis_square_80181228(Task*);
void func_acropolis_square_80182048(Task*);

TaskDesc D_acropolis_square_801837A0[3] = {
    { 0, 32, func_acropolis_square_80181228, { .model = NULL } },
    { 0, 32, func_acropolis_square_80182048, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_acropolis_square_801837C4[6] = {
    { 5102, func_acropolis_square_80181794 },
    { 5103, func_acropolis_square_801820D8 },
    { 5105, func_acropolis_square_80182108 },
    { 5104, func_acropolis_square_801819BC },
    { 5106, func_acropolis_square_80182110 },
    { 0x7FFFFFFF, NULL },
};

GpAnimArg D_acropolis_square_801837F4 = { { .index = 1 }, 1, 0, 0, 0 };

TaskDesc D_acropolis_square_80183808[3] = {
    { 0, 32, func_acropolis_square_80181AEC, { .model = NULL } },
    { 0, 32, func_acropolis_square_80182148, { .model = NULL } },
    { 0, 192, func_acropolis_square_80181DD0, { .model = NULL } },
};

s32 D_acropolis_square_8018382C = 0;

s32 D_acropolis_square_80183830 = 0;

GpEvsCmd D_acropolis_square_80183834[7] = {
    { 1, { .value = 5 }, { .value = 0 }, { .value = 3103 }, { .value = 1 }, { .value = 0 } },
    { 3, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5101000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 148 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_square_801838DC[8] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_square_8018399C[8] = {
    { 3, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_square_801837F4 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_square_80182200 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 395 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_square_80183A5C[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_square_80182200 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_acropolis_square_80183B34[9] = {
    0,
    0x51010001,
    0,
    0,
    0,
    0x51010005,
    0x51010006,
    0x51010007,
    0x51010008,
};

s32 func_acropolis_square_8018344C(s32, s32, s32);

AcropolisSquareMessageEntry D_acropolis_square_80183B58[2] = {
    { 3103, { .call0 = func_acropolis_square_8018344C } },
    { 2147483647, { .call0 = NULL } },
};

s16 D_acropolis_square_80183B68[24] = {
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    3784,
    4096,
    3784,
    2896,
    1567,
    0,
    -1567,
    -2896,
    -3784,
    -4096,
    -3784,
    -2896,
    -1567,
    0,
    1567,
    2896,
    0,
};

s32 D_acropolis_square_80183B98 = 0;

GpRoomObjRec D_acropolis_square_80183B9C[1] = {
    { D_acropolis_square_8018519C, D_acropolis_square_801851C0, D_acropolis_square_80185680, NULL },
};

u8 * D_acropolis_square_80183BAC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_acropolis_square_80183BB0[1] = {
    { { .bytes = { 15, 0 } } },
};

GpRoomCoordRec D_acropolis_square_80183BB4[1] = {
    { D_acropolis_square_80186468, D_acropolis_square_80186480 },
};

GpWarpRec D_acropolis_square_80183BBC[7] = {
    { { .words = { 1024, -6510, -2136, 511 } }, { 0, 0, 0, 0 }, { .words = { 1024, -6510, -2136, 511 } }, { 0, 0, 0, 0 }, 0x51010003, 0x51010002, 0x51010004, 13, 0, 503 },
    { { .words = { 3072, 6513, -2138, 140 } }, { 0, 0, 0, 0 }, { .words = { 3072, 6513, -2138, 140 } }, { 0, 0, 0, 0 }, 0x51010003, 0x51010002, 0x51010004, 6, 0, 502 },
    { { .words = { 256, -4227, -1535, -3725 } }, { 0, 0, 0, 0 }, { .words = { 256, -4227, -1535, -3725 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 0, 0 },
    { { .words = { 3840, 4199, -1535, -3916 } }, { 0, 0, 0, 0 }, { .words = { 3840, 4199, -1535, -3916 } }, { 0, 0, 0, 0 }, 0, 0, 0, 5, 0, 0 },
    { { .words = { 256, -4703, -1000, -4644 } }, { 0, 0, 0, 0 }, { .words = { 256, -4419, -1200, -4340 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 1, 0 },
    { { .words = { 3840, 4905, -1000, -4576 } }, { 0, 0, 0, 0 }, { .words = { 3840, 4481, -1240, -4416 } }, { 0, 0, 0, 0 }, 0, 0, 0, 5, 1, 0 },
    { { .words = { 1024, -6510, -2136, 511 } }, { 0, 0, 0, 0 }, { .words = { 1024, -6510, -2136, 511 } }, { 0, 0, 0, 0 }, 0x51010003, 0x51010002, 0x51010004, 15, 0, 503 },
};

SVECTOR D_acropolis_square_80183D44[82] = {
    { 0, -4096, 0, 0 },
    { 1295, 0, 3886, 0 },
    { 2527, 0, -3223, 0 },
    { -2527, 0, -3223, 0 },
    { -2579, 0, 3182, 0 },
    { -1871, 0, 3644, 0 },
    { 3850, 0, -1399, 0 },
    { -4096, 0, 0, 0 },
    { 4094, 0, 128, 0 },
    { 0, 0, -4096, 0 },
    { -4084, 0, -309, 0 },
    { -1418, 0, 3843, 0 },
    { 1400, 0, 3849, 0 },
    { 0, 0, 4096, 0 },
    { 2747, 0, 3039, 0 },
    { 4090, 0, -229, 0 },
    { -1566, 0, -3785, 0 },
    { 553, 0, 4058, 0 },
    { 3785, 0, 1566, 0 },
    { -4060, 0, 541, 0 },
    { 3785, 0, -1566, 0 },
    { -3785, 0, -1566, 0 },
    { -574, 0, -4056, 0 },
    { 4061, 0, -534, 0 },
    { 1566, 0, 3785, 0 },
    { -3785, 0, 1566, 0 },
    { 1566, 0, -3785, 0 },
    { 4096, 0, 0, 0 },
    { -1566, 0, 3785, 0 },
    { -853, -3272, -2311, 0 },
    { 676, 0, 4040, 0 },
    { 855, -3267, -2318, 0 },
    { -650, 0, 4044, 0 },
    { 574, 0, -4056, 0 },
    { 0, -2740, -3045, 0 },
    { -4061, 0, -534, 0 },
    { -1418, 0, -3843, 0 },
    { 1411, 0, -3845, 0 },
    { 4064, 0, -508, 0 },
    { -4056, 0, -568, 0 },
    { -803, 0, -4016, 0 },
    { 803, 0, -4016, 0 },
    { -553, 0, 4058, 0 },
    { -292, 0, -4086, 0 },
    { 292, 0, -4086, 0 },
    { -3850, 0, -1399, 0 },
    { -4094, 0, 128, 0 },
    { -1340, 0, 3871, 0 },
    { 3831, 0, 1449, 0 },
    { 4060, 0, 541, 0 },
    { -3833, 0, 1444, 0 },
    { -3472, 0, -2173, 0 },
    { 80, 0, -4095, 0 },
    { 3808, 0, -1508, 0 },
    { -684, 0, 4038, 0 },
    { -3433, 0, 2234, 0 },
    { 2378, 0, 3335, 0 },
    { -3403, 0, -2280, 0 },
    { -1133, 0, -3936, 0 },
    { 2948, 0, -2844, 0 },
    { -3443, 0, -2219, 0 },
    { 319, 0, -4084, 0 },
    { 4045, 0, -644, 0 },
    { 1159, 0, 3929, 0 },
    { -3553, 0, -2038, 0 },
    { -347, 0, -4081, 0 },
    { 2509, 0, -3238, 0 },
    { -3418, 0, 2257, 0 },
    { 577, 0, -4055, 0 },
    { 4082, 0, -343, 0 },
    { 1730, 0, 3713, 0 },
    { -4048, 0, 622, 0 },
    { -1564, 0, 3786, 0 },
    { 1575, 0, 3781, 0 },
    { -368, 0, 4079, 0 },
    { 4075, 0, -418, 0 },
    { 2928, 0, 2865, 0 },
    { 4014, 0, -814, 0 },
    { -810, 0, 4015, 0 },
    { -4054, 0, 583, 0 },
    { -2272, 0, -3408, 0 },
    { -2896, 0, 2896, 0 },
};

SVECTOR D_acropolis_square_80183FD4[262] = {
    { -7000, -1600, -3640, 0 },
    { -7000, -1600, -2000, 0 },
    { -5260, -1600, -2000, 0 },
    { -5290, -1600, -3750, 0 },
    { -3370, -1600, -2000, 0 },
    { -3420, -1600, -4430, 0 },
    { 3370, -1600, -2000, 0 },
    { 3420, -1600, -4440, 0 },
    { 5290, -1600, -3750, 0 },
    { 5260, -1600, -2000, 0 },
    { 7000, -1600, -2000, 0 },
    { 7000, -1600, -3640, 0 },
    { 5970, -1600, -5610, 0 },
    { 7000, -1600, -5610, 0 },
    { -7000, -1600, -5610, 0 },
    { -5970, -1600, -5610, 0 },
    { -4100, -1600, -6300, 0 },
    { 4100, -1600, -6300, 0 },
    { 4100, -1600, -8000, 0 },
    { -4100, -1600, -8000, 0 },
    { -7000, -2200, 6000, 0 },
    { 7000, -2200, 6000, 0 },
    { 7000, -2200, -1920, 0 },
    { -7000, -2200, -1920, 0 },
    { -3180, 190, -4510, 0 },
    { -3180, -2600, -4510, 0 },
    { -3420, -2600, -4430, 0 },
    { -3420, 190, -4430, 0 },
    { 1100, -1510, -4420, 0 },
    { 1100, -3000, -4420, 0 },
    { 1980, -3000, -3730, 0 },
    { 1980, -1510, -3730, 0 },
    { -1980, -1510, -3730, 0 },
    { -1980, -3000, -3730, 0 },
    { -1100, -3000, -4420, 0 },
    { -1100, -1510, -4420, 0 },
    { 5180, -2600, -3040, 0 },
    { 5180, 190, -3040, 0 },
    { 5760, 190, -2570, 0 },
    { 5760, -2600, -2570, 0 },
    { 5520, -3040, -1690, 0 },
    { 5520, -1000, -1690, 0 },
    { 7000, -1000, -930, 0 },
    { 7000, -3045, -930, 0 },
    { -6100, 190, -5920, 0 },
    { -6100, -2600, -5920, 0 },
    { -5170, -2600, -3360, 0 },
    { -5170, 190, -3360, 0 },
    { 5520, -3040, -3010, 0 },
    { 5520, -1000, -3010, 0 },
    { -5180, -2600, -3040, 0 },
    { -5180, 190, -3040, 0 },
    { 7000, -3040, -3010, 0 },
    { 7000, -1000, -3010, 0 },
    { 3275, -2600, -6050, 0 },
    { 3275, 190, -6050, 0 },
    { 3160, 190, -4530, 0 },
    { 3160, -2600, -4530, 0 },
    { 5290, -1800, -3750, 0 },
    { 3420, -1800, -4440, 0 },
    { -5290, -1800, -3750, 0 },
    { -3420, -1800, -4430, 0 },
    { -7000, -2400, -1920, 0 },
    { 7000, -2400, -1920, 0 },
    { -5700, -2600, -2570, 0 },
    { -5700, 190, -2570, 0 },
    { -5530, -1000, -1070, 0 },
    { -5530, -3045, -1070, 0 },
    { -7000, -3045, -1070, 0 },
    { -7000, -1000, -1070, 0 },
    { -3265, 190, -6030, 0 },
    { -3265, -2600, -6030, 0 },
    { 3060, -2630, 3820, 0 },
    { 5040, -2630, 3550, 0 },
    { 4940, -2630, 2790, 0 },
    { 2960, -2630, 3070, 0 },
    { -1740, -1570, -3740, 0 },
    { -1740, -3000, -3740, 0 },
    { 0, -3000, -4460, 0 },
    { 0, -1570, -4460, 0 },
    { 3060, -2200, 3820, 0 },
    { 5040, -2200, 3550, 0 },
    { 2460, -1570, -2000, 0 },
    { 2460, -3000, -2000, 0 },
    { 1740, -3000, -260, 0 },
    { 1740, -1570, -260, 0 },
    { 2960, -2200, 3070, 0 },
    { 1740, -1570, -3740, 0 },
    { 1740, -3000, -3740, 0 },
    { -2460, -1570, -2000, 0 },
    { -2460, -3000, -2000, 0 },
    { 4940, -2200, 2790, 0 },
    { 0, -3000, 460, 0 },
    { 0, -1570, 460, 0 },
    { -1000, -2630, 3890, 0 },
    { 1000, -2630, 3890, 0 },
    { 1000, -2630, 3130, 0 },
    { -1000, -2630, 3130, 0 },
    { -1740, -1570, -260, 0 },
    { -1740, -3000, -260, 0 },
    { 1000, -2200, 3890, 0 },
    { -1000, -2200, 3890, 0 },
    { 1000, -2200, 3130, 0 },
    { -4050, -200, -6180, 0 },
    { -5920, -200, -5490, 0 },
    { 20, 190, -6580, 0 },
    { 20, -2600, -6580, 0 },
    { 4050, -200, -6180, 0 },
    { 5930, -200, -5490, 0 },
    { -20, -2600, -6580, 0 },
    { -20, 190, -6580, 0 },
    { -4940, -2200, 2790, 0 },
    { -4940, -2630, 2790, 0 },
    { -2960, -2630, 3070, 0 },
    { -2960, -2200, 3070, 0 },
    { -7000, -1600, -2280, 0 },
    { -7000, -1800, -2280, 0 },
    { 7000, -1800, -2280, 0 },
    { 7000, -1600, -2280, 0 },
    { -5040, -2200, 3550, 0 },
    { -5040, -2630, 3550, 0 },
    { -5920, 0, -5490, 0 },
    { -4050, 0, -6180, 0 },
    { 5930, 0, -5490, 0 },
    { 4050, 0, -6180, 0 },
    { 2100, -3000, -2770, 0 },
    { 2100, -1510, -2770, 0 },
    { -2120, -1510, -2730, 0 },
    { -2120, -3000, -2730, 0 },
    { 0, -3000, -4640, 0 },
    { 0, -1510, -4640, 0 },
    { -590, -2590, -1400, 0 },
    { 590, -2590, -1400, 0 },
    { 590, -7430, -1400, 0 },
    { -590, -7430, -1400, 0 },
    { 590, -2590, -2590, 0 },
    { -590, -2590, -2590, 0 },
    { -590, -7430, -2590, 0 },
    { 590, -7430, -2590, 0 },
    { -1000, -2200, 3130, 0 },
    { -3060, -2200, 3820, 0 },
    { -3060, -2630, 3820, 0 },
    { 7000, -6890, 4900, 0 },
    { 7000, -2200, 4900, 0 },
    { 0, -2200, 5400, 0 },
    { 0, -6890, 5400, 0 },
    { -7000, -2200, 4900, 0 },
    { -7000, -6890, 4900, 0 },
    { 6100, -2600, -5920, 0 },
    { 6100, 190, -5920, 0 },
    { 5170, 190, -3360, 0 },
    { 5170, -2600, -3360, 0 },
    { 7000, -6835, 4690, 0 },
    { 7000, -2200, 4690, 0 },
    { 0, -2200, 5190, 0 },
    { 0, -6830, 5190, 0 },
    { -7000, -2200, 4690, 0 },
    { -7000, -6830, 4690, 0 },
    { -6750, -1600, -4020, 0 },
    { -6750, -6690, -4020, 0 },
    { -6750, -6690, 6000, 0 },
    { -6750, -1600, 6000, 0 },
    { -7000, -1600, -4020, 0 },
    { -7000, -6690, -4020, 0 },
    { 6750, -6690, -4020, 0 },
    { 6750, -1600, -4020, 0 },
    { 6750, -1600, 5000, 0 },
    { 6750, -6690, 5000, 0 },
    { 7000, -6690, -4020, 0 },
    { 7000, -1600, -4020, 0 },
    { -6290, -2200, 1160, 0 },
    { -6290, -6700, 1160, 0 },
    { -6290, -6700, 6000, 0 },
    { -6290, -2200, 6000, 0 },
    { -6790, -2200, 1160, 0 },
    { -6790, -6700, 1160, 0 },
    { 3420, 190, -4440, 0 },
    { 3420, -2600, -4440, 0 },
    { 4290, 190, -6740, 0 },
    { 4290, -2600, -6740, 0 },
    { -5530, -1000, -3010, 0 },
    { -5530, -3045, -3010, 0 },
    { -7000, -1000, -3010, 0 },
    { -7000, -3045, -3010, 0 },
    { -4290, -2600, -6740, 0 },
    { -4290, 190, -6740, 0 },
    { 2485, -1610, 3180, 0 },
    { 2485, -3180, 3180, 0 },
    { 3045, -3180, 2285, 0 },
    { 3045, -1610, 2285, 0 },
    { 4835, -3180, 2320, 0 },
    { 4835, -1610, 2320, 0 },
    { 5130, -3180, 3065, 0 },
    { 5130, -1610, 3065, 0 },
    { 3640, -1610, 4128, 0 },
    { 3640, -3180, 4128, 0 },
    { 3035, -3180, 4025, 0 },
    { 3035, -1610, 4025, 0 },
    { 875, -1585, 5555, 0 },
    { 875, -2940, 5555, 0 },
    { 1220, -2940, 5040, 0 },
    { 1220, -1585, 5040, 0 },
    { 2175, -2940, 4765, 0 },
    { 2175, -1585, 4765, 0 },
    { 2990, -2940, 5610, 0 },
    { 2990, -1585, 5610, 0 },
    { -280, -665, 3790, 0 },
    { -280, -2895, 3790, 0 },
    { 400, -2895, 2735, 0 },
    { 400, -665, 2735, 0 },
    { 1105, -2895, 2790, 0 },
    { 1105, -665, 2790, 0 },
    { 1195, -2895, 3355, 0 },
    { 1195, -665, 3355, 0 },
    { -2010, -1585, 5605, 0 },
    { -2010, -2630, 5605, 0 },
    { -1620, -2630, 4925, 0 },
    { -1620, -1585, 4925, 0 },
    { -915, -2630, 4865, 0 },
    { -915, -1585, 4865, 0 },
    { 40, -2630, 5605, 0 },
    { 40, -1585, 5605, 0 },
    { -3410, -1610, 4075, 0 },
    { -3410, -2705, 4075, 0 },
    { -4110, -2705, 3015, 0 },
    { -4110, -1610, 3015, 0 },
    { -2950, -2705, 3180, 0 },
    { -2950, -1610, 3180, 0 },
    { -2895, -2705, 3835, 0 },
    { -2895, -1610, 3835, 0 },
    { -2065, -1570, 40, 0 },
    { -2065, -2705, 40, 0 },
    { -2360, -2705, -1880, 0 },
    { -2360, -1570, -1880, 0 },
    { -1460, -1570, 290, 0 },
    { -1460, -2705, 290, 0 },
    { -800, -1570, 15, 0 },
    { -800, -2705, 15, 0 },
    { 1640, 0, 380, 0 },
    { 1640, -2700, 380, 0 },
    { 255, -2700, 255, 0 },
    { 255, 0, 255, 0 },
    { 2190, 0, -1635, 0 },
    { 2190, -2700, -1635, 0 },
    { 2325, -2700, -320, 0 },
    { 2325, 0, -320, 0 },
    { -6110, -2995, -1110, 0 },
    { -4945, -2995, -875, 0 },
    { -4945, -2995, -1990, 0 },
    { -5225, -2995, -3370, 0 },
    { -5225, -1615, -3370, 0 },
    { -4945, -1615, -1990, 0 },
    { -4945, -1615, -875, 0 },
    { -6110, -1615, -1110, 0 },
    { 6480, -1600, 2550, 0 },
    { 6480, -5000, 2550, 0 },
    { 6280, -5000, 1160, 0 },
    { 6280, -1600, 1160, 0 },
    { 7000, -5000, 680, 0 },
    { 7000, -1600, 680, 0 },
    { 7000, -1600, 3070, 0 },
    { 7000, -5000, 3070, 0 },
};

GpGridFace D_acropolis_square_80184804[115] = {
    { { 1, 2, 0, 3 }, 0, 4 },
    { { 2, 4, 3, 5 }, 0, 4 },
    { { 4, 6, 5, 7 }, 0, 4 },
    { { 9, 10, 8, 11 }, 0, 4 },
    { { 6, 9, 7, 8 }, 0, 4 },
    { { 8, 11, 12, 13 }, 0, 4 },
    { { 0, 3, 14, 15 }, 0, 4 },
    { { 5, 7, 16, 17 }, 0, 4 },
    { { 18, 19, 17, 16 }, 0, 4 },
    { { 21, 22, 20, 23 }, 0, 4 },
    { { 25, 26, 24, 27 }, 1, 0 },
    { { 29, 30, 28, 31 }, 2, 4 },
    { { 33, 34, 32, 35 }, 3, 4 },
    { { 37, 38, 36, 39 }, 4, 0 },
    { { 41, 42, 40, 43 }, 5, 0 },
    { { 45, 46, 44, 47 }, 6, 0 },
    { { 49, 41, 48, 40 }, 7, 0 },
    { { 46, 50, 47, 51 }, 8, 0 },
    { { 53, 49, 52, 48 }, 9, 0 },
    { { 55, 56, 54, 57 }, 10, 0 },
    { { 58, 59, 8, 7 }, 11, 4 },
    { { 3, 5, 60, 61 }, 12, 4 },
    { { 23, 22, 62, 63 }, 13, 4 },
    { { 50, 64, 51, 65 }, 14, 0 },
    { { 67, 68, 66, 69 }, 13, 0 },
    { { 71, 25, 70, 24 }, 15, 0 },
    { { 73, 74, 72, 75 }, 0, 0 },
    { { 77, 78, 76, 79 }, 16, 0 },
    { { 80, 81, 72, 73 }, 17, 0 },
    { { 83, 84, 82, 85 }, 18, 0 },
    { { 86, 80, 75, 72 }, 19, 0 },
    { { 88, 83, 87, 82 }, 20, 0 },
    { { 90, 77, 89, 76 }, 21, 0 },
    { { 91, 86, 74, 75 }, 22, 0 },
    { { 81, 91, 73, 74 }, 23, 0 },
    { { 84, 92, 85, 93 }, 24, 0 },
    { { 95, 96, 94, 97 }, 0, 0 },
    { { 99, 90, 98, 89 }, 25, 0 },
    { { 95, 94, 100, 101 }, 13, 0 },
    { { 78, 88, 79, 87 }, 26, 0 },
    { { 96, 95, 102, 100 }, 27, 0 },
    { { 92, 99, 93, 98 }, 28, 0 },
    { { 103, 104, 5, 3 }, 29, 4 },
    { { 70, 105, 71, 106 }, 30, 0 },
    { { 7, 8, 107, 108 }, 31, 4 },
    { { 54, 109, 55, 110 }, 32, 0 },
    { { 112, 113, 111, 114 }, 33, 0 },
    { { 116, 117, 115, 118 }, 9, 4 },
    { { 22, 117, 23, 116 }, 34, 4 },
    { { 120, 112, 119, 111 }, 35, 0 },
    { { 104, 103, 121, 122 }, 36, 4 },
    { { 123, 124, 108, 107 }, 37, 4 },
    { { 126, 31, 125, 30 }, 38, 4 },
    { { 128, 33, 127, 32 }, 39, 4 },
    { { 34, 129, 35, 130 }, 40, 4 },
    { { 132, 133, 131, 134 }, 13, 0 },
    { { 136, 137, 135, 138 }, 9, 0 },
    { { 129, 29, 130, 28 }, 41, 4 },
    { { 138, 133, 135, 132 }, 27, 0 },
    { { 94, 97, 101, 139 }, 7, 0 },
    { { 97, 96, 139, 102 }, 9, 0 },
    { { 134, 137, 131, 136 }, 7, 0 },
    { { 141, 120, 140, 119 }, 42, 0 },
    { { 141, 113, 120, 112 }, 0, 0 },
    { { 143, 144, 142, 145 }, 43, 0 },
    { { 147, 145, 146, 144 }, 44, 0 },
    { { 149, 150, 148, 151 }, 45, 0 },
    { { 153, 154, 152, 155 }, 43, 0 },
    { { 150, 37, 151, 36 }, 46, 0 },
    { { 157, 155, 156, 154 }, 44, 0 },
    { { 159, 160, 158, 161 }, 27, 0 },
    { { 163, 159, 162, 158 }, 9, 0 },
    { { 165, 166, 164, 167 }, 7, 0 },
    { { 169, 165, 168, 164 }, 9, 0 },
    { { 171, 172, 170, 173 }, 27, 0 },
    { { 175, 171, 174, 170 }, 9, 0 },
    { { 56, 176, 57, 177 }, 47, 0 },
    { { 176, 178, 177, 179 }, 48, 0 },
    { { 113, 141, 114, 140 }, 49, 0 },
    { { 181, 67, 180, 66 }, 27, 0 },
    { { 183, 181, 182, 180 }, 9, 0 },
    { { 26, 184, 27, 185 }, 50, 0 },
    { { 187, 188, 186, 189 }, 51, 0 },
    { { 188, 190, 189, 191 }, 52, 0 },
    { { 190, 192, 191, 193 }, 53, 0 },
    { { 195, 196, 194, 197 }, 54, 0 },
    { { 186, 197, 187, 196 }, 55, 0 },
    { { 194, 193, 195, 192 }, 56, 0 },
    { { 199, 200, 198, 201 }, 57, 0 },
    { { 200, 202, 201, 203 }, 58, 0 },
    { { 202, 204, 203, 205 }, 59, 0 },
    { { 207, 208, 206, 209 }, 60, 0 },
    { { 208, 210, 209, 211 }, 61, 0 },
    { { 210, 212, 211, 213 }, 62, 0 },
    { { 212, 207, 213, 206 }, 63, 0 },
    { { 215, 216, 214, 217 }, 64, 0 },
    { { 216, 218, 217, 219 }, 65, 0 },
    { { 218, 220, 219, 221 }, 66, 0 },
    { { 223, 224, 222, 225 }, 67, 0 },
    { { 224, 226, 225, 227 }, 68, 0 },
    { { 226, 228, 227, 229 }, 69, 0 },
    { { 228, 223, 229, 222 }, 70, 0 },
    { { 231, 232, 230, 233 }, 71, 0 },
    { { 235, 231, 234, 230 }, 72, 0 },
    { { 237, 235, 236, 234 }, 73, 0 },
    { { 239, 240, 238, 241 }, 74, 0 },
    { { 243, 244, 242, 245 }, 75, 0 },
    { { 244, 239, 245, 238 }, 76, 0 },
    { { 247, 248, 246, 249 }, 0, 0 },
    { { 249, 248, 250, 251 }, 77, 0 },
    { { 248, 247, 251, 252 }, 27, 0 },
    { { 247, 246, 252, 253 }, 78, 0 },
    { { 255, 256, 254, 257 }, 79, 0 },
    { { 256, 258, 257, 259 }, 80, 0 },
    { { 261, 255, 260, 254 }, 81, 0 },
};

s16 D_acropolis_square_80184D68[32] = {
    0,
    1,
    2,
    6,
    7,
    8,
    9,
    10,
    12,
    15,
    17,
    21,
    22,
    23,
    25,
    27,
    32,
    42,
    43,
    47,
    48,
    50,
    53,
    70,
    71,
    79,
    80,
    81,
    108,
    109,
    110,
    -1,
};

s16 D_acropolis_square_80184DA8[38] = {
    0,
    1,
    2,
    6,
    7,
    9,
    10,
    12,
    15,
    17,
    21,
    22,
    23,
    24,
    25,
    27,
    32,
    37,
    41,
    42,
    47,
    48,
    50,
    53,
    70,
    71,
    74,
    75,
    79,
    80,
    81,
    102,
    103,
    108,
    109,
    110,
    111,
    -1,
};

s16 D_acropolis_square_80184DF4[31] = {
    0,
    1,
    9,
    22,
    24,
    37,
    41,
    46,
    48,
    49,
    62,
    63,
    65,
    69,
    70,
    74,
    75,
    78,
    79,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    108,
    109,
    110,
    111,
    -1,
};

s16 D_acropolis_square_80184E34[17] = {
    9,
    46,
    49,
    62,
    63,
    65,
    69,
    70,
    74,
    78,
    95,
    96,
    98,
    99,
    100,
    101,
    -1,
};

s16 D_acropolis_square_80184E58[30] = {
    1,
    2,
    7,
    8,
    9,
    10,
    11,
    12,
    21,
    22,
    25,
    27,
    31,
    32,
    39,
    42,
    43,
    45,
    47,
    48,
    50,
    52,
    53,
    54,
    56,
    57,
    58,
    61,
    81,
    -1,
};

s16 D_acropolis_square_80184E94[41] = {
    1,
    2,
    7,
    9,
    10,
    11,
    12,
    21,
    22,
    25,
    27,
    29,
    31,
    32,
    35,
    37,
    39,
    41,
    42,
    47,
    48,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    61,
    81,
    102,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    111,
    -1,
};

s16 D_acropolis_square_80184EE8[51] = {
    2,
    9,
    22,
    26,
    29,
    30,
    33,
    35,
    36,
    37,
    38,
    40,
    41,
    46,
    48,
    49,
    55,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    67,
    69,
    78,
    82,
    83,
    86,
    88,
    89,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    -1,
};

s16 D_acropolis_square_80184F50[29] = {
    9,
    36,
    38,
    40,
    46,
    59,
    60,
    62,
    63,
    64,
    65,
    67,
    69,
    78,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    -1,
};

s16 D_acropolis_square_80184F8C[32] = {
    2,
    3,
    4,
    5,
    7,
    8,
    9,
    11,
    13,
    16,
    18,
    19,
    20,
    22,
    27,
    29,
    31,
    39,
    43,
    44,
    45,
    47,
    48,
    51,
    52,
    54,
    57,
    66,
    68,
    76,
    77,
    -1,
};

s16 D_acropolis_square_80184FCC[40] = {
    2,
    3,
    4,
    5,
    7,
    9,
    11,
    13,
    14,
    16,
    18,
    19,
    20,
    22,
    27,
    29,
    31,
    35,
    39,
    41,
    44,
    45,
    47,
    48,
    52,
    54,
    55,
    56,
    57,
    58,
    61,
    66,
    68,
    72,
    76,
    77,
    105,
    106,
    107,
    -1,
};

s16 D_acropolis_square_8018501C[41] = {
    2,
    4,
    9,
    22,
    26,
    28,
    29,
    30,
    31,
    33,
    34,
    35,
    36,
    38,
    40,
    41,
    48,
    60,
    64,
    67,
    72,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    105,
    106,
    107,
    112,
    113,
    114,
    -1,
};

s16 D_acropolis_square_80185070[31] = {
    9,
    26,
    28,
    30,
    33,
    34,
    36,
    38,
    40,
    60,
    64,
    65,
    67,
    69,
    72,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    96,
    97,
    -1,
};

s16 D_acropolis_square_801850B0[25] = {
    2,
    3,
    4,
    5,
    7,
    8,
    9,
    13,
    16,
    18,
    19,
    20,
    22,
    44,
    45,
    47,
    48,
    51,
    66,
    68,
    72,
    73,
    76,
    77,
    -1,
};

s16 D_acropolis_square_801850E4[22] = {
    2,
    3,
    4,
    5,
    9,
    13,
    14,
    16,
    18,
    20,
    22,
    44,
    47,
    48,
    51,
    66,
    68,
    72,
    73,
    112,
    113,
    -1,
};

s16 D_acropolis_square_80185110[22] = {
    3,
    9,
    14,
    16,
    22,
    26,
    28,
    33,
    34,
    48,
    64,
    67,
    72,
    82,
    83,
    84,
    85,
    87,
    112,
    113,
    114,
    -1,
};

s16 D_acropolis_square_8018513C[15] = {
    9,
    26,
    28,
    33,
    34,
    64,
    67,
    72,
    84,
    85,
    87,
    90,
    112,
    114,
    -1,
};

s16 * D_acropolis_square_8018515C[16] = {
    D_acropolis_square_80184D68,
    D_acropolis_square_80184DA8,
    D_acropolis_square_80184DF4,
    D_acropolis_square_80184E34,
    D_acropolis_square_80184E58,
    D_acropolis_square_80184E94,
    D_acropolis_square_80184EE8,
    D_acropolis_square_80184F50,
    D_acropolis_square_80184F8C,
    D_acropolis_square_80184FCC,
    D_acropolis_square_8018501C,
    D_acropolis_square_80185070,
    D_acropolis_square_801850B0,
    D_acropolis_square_801850E4,
    D_acropolis_square_80185110,
    D_acropolis_square_8018513C,
};

GpGridParams D_acropolis_square_8018519C[1] = {
    { NULL, D_acropolis_square_80183D44, D_acropolis_square_80183FD4, D_acropolis_square_80184804, D_acropolis_square_8018515C, 7000, 8000, 4, 4, 4000, 115 },
};

GpObj4C D_acropolis_square_801851C0[16] = {
    { NULL, NULL, NULL, { -2050, -3681, 2655, 0 }, { { -1080, 2717, 3957, 0 }, { 1070, 2724, -3953, 0 }, { -1074, -2723, 3949, 0 }, { 1075, -2716, -3960, 0 } }, { 3952, 2, 1074, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 2253, -3648, -4067, 0 }, { { -1346, 2813, 704, 0 }, { 1332, 2628, -777, 0 }, { -1339, -2627, 707, 0 }, { 1336, -2812, -780, 0 } }, { 1986, 4, 3590, 0 }, { 0, 4096, 0, 0 }, 3197, 0, 5, 3, 1, 0 },
    { NULL, NULL, NULL, { -1682, -3744, -3572, 0 }, { { 1535, 2813, 886, 0 }, { -1565, 2628, -890, 0 }, { 1535, -2627, 883, 0 }, { -1563, -2812, -896, 0 } }, { 2045, 1, -3574, 0 }, { 0, 4096, 0, 0 }, 3328, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 2267, -3584, -3843, 0 }, { { 1497, 2813, -816, 0 }, { -1503, 2628, 766, 0 }, { 1486, -2627, -817, 0 }, { -1510, -2812, 762, 0 } }, { -1917, 3, -3632, 0 }, { 0, 4096, 0, 0 }, 3278, 0, 3, 5, 1, 0 },
    { NULL, NULL, NULL, { 4159, -3648, -770, 0 }, { { 3327, 2718, 116, 0 }, { -3328, 2723, -122, 0 }, { 3321, -2722, 116, 0 }, { -3335, -2717, -124, 0 } }, { 146, -1, -4110, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 4222, -3648, -1250, 0 }, { { -3337, 2718, -120, 0 }, { 3322, 2723, 111, 0 }, { -3331, -2722, -117, 0 }, { 3329, -2717, 111, 0 } }, { -143, 1, 4111, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { -1635, -3712, 3102, 0 }, { { 763, 2718, -3250, 0 }, { -772, 2723, 3235, 0 }, { 761, -2722, -3245, 0 }, { -772, -2717, 3241, 0 } }, { -4005, 0, -949, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 1660, -3616, 2814, 0 }, { { 531, 2718, 3289, 0 }, { -533, 2723, -3287, 0 }, { 531, -2722, 3285, 0 }, { -535, -2717, -3293, 0 } }, { 4060, 0, -658, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 2269, -3680, 2750, 0 }, { { -697, 2718, -3264, 0 }, { 688, 2723, 3252, 0 }, { -696, -2722, -3259, 0 }, { 690, -2717, 3258, 0 } }, { -4025, 0, 855, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { -4579, -2464, -2338, 0 }, { { -4100, 2717, 117, 0 }, { 4094, 2724, -120, 0 }, { -4094, -2723, 119, 0 }, { 4100, -2716, -116, 0 } }, { 118, 1, 4094, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 8, 4, 1, 0 },
    { NULL, NULL, NULL, { -4675, -2432, -2050, 0 }, { { 4101, 2717, -116, 0 }, { -4094, 2724, 120, 0 }, { 4094, -2723, -119, 0 }, { -4100, -2716, 117, 0 } }, { -119, 2, -4095, 0 }, { 0, 4096, 0, 0 }, 4910, 0, 4, 8, 1, 0 },
    { NULL, NULL, NULL, { -5648, -3456, 1024, 0 }, { { -859, 2717, 326, 0 }, { 850, 2724, -323, 0 }, { -848, -2723, 324, 0 }, { 859, -2716, -324, 0 } }, { 1460, 0, 3847, 0 }, { 0, 4096, 0, 0 }, 2862, 0, 8, 13, 1, 0 },
    { NULL, NULL, NULL, { -5249, -3488, -211, 0 }, { { 406, 2717, 983, 0 }, { -403, 2724, -972, 0 }, { 404, -2723, 973, 0 }, { -406, -2716, -981, 0 } }, { 3791, 1, -1570, 0 }, { 0, 4096, 0, 0 }, 2918, 0, 8, 13, 1, 0 },
    { NULL, NULL, NULL, { -5459, -3552, 1054, 0 }, { { 955, 2717, -367, 0 }, { -946, 2724, 365, 0 }, { 946, -2723, -364, 0 }, { -954, -2716, 368, 0 } }, { -1478, 0, -3835, 0 }, { 0, 4096, 0, 0 }, 2896, 0, 13, 8, 1, 0 },
    { NULL, NULL, NULL, { -5154, -3488, -306, 0 }, { { -482, 2717, -1106, 0 }, { 480, 2724, 1096, 0 }, { -480, -2723, -1096, 0 }, { 482, -2716, 1105, 0 } }, { -3758, 1, 1640, 0 }, { 0, 4096, 0, 0 }, 2974, 0, 13, 8, 1, 0 },
    { NULL, NULL, NULL, { -1696, -3648, -3826, 0 }, { { -1574, 2813, -843, 0 }, { 1560, 2628, 827, 0 }, { -1574, -2627, -836, 0 }, { 1560, -2812, 836, 0 } }, { -1936, 4, 3632, 0 }, { 0, 4096, 0, 0 }, 3318, 0, 4, 3, 129, 0 },
};

GpObj4C D_acropolis_square_80185680[26] = {
    { NULL, NULL, NULL, { -6416, -2240, 0, 0 }, { { -432, 0, -1088, 0 }, { 432, 0, -1088, 0 }, { -432, 0, 1088, 0 }, { 432, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1166, 0, 17, 18, 2, 0 },
    { NULL, NULL, NULL, { 6320, -2237, -64, 0 }, { { -480, 0, -1024, 0 }, { 480, 0, -1024, 0 }, { -480, 0, 1024, 0 }, { 480, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1130, 0, 2, 33, 2, 0 },
    { NULL, NULL, NULL, { -4161, -1664, -3873, 0 }, { { 489, 0, -473, 0 }, { 672, 0, 40, 0 }, { -671, 0, -39, 0 }, { -488, 0, 474, 0 } }, { 0, 4096, 0, 0 }, { 1189, 0, 3920, 0 }, 680, 257, 70, 144, 2, 0 },
    { NULL, NULL, NULL, { 4286, -1667, -3969, 0 }, { { 704, 0, -39, 0 }, { 521, 0, 473, 0 }, { -519, 0, -473, 0 }, { -703, 0, 40, 0 } }, { 0, 4105, 0, 0 }, { -1380, 0, 3857, 0 }, 704, 257, 70, 112, 2, 0 },
    { NULL, NULL, NULL, { -4528, -2242, -1728, 0 }, { { 2128, 0, -176, 0 }, { 2128, 0, 176, 0 }, { -2128, 0, -176, 0 }, { -2128, 0, 176, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 2126, 257, 67, 128, 2, 0 },
    { NULL, NULL, NULL, { -4753, -1632, -2481, 0 }, { { 2188, 0, -166, 0 }, { 2197, 0, 186, 0 }, { -2196, 0, -185, 0 }, { -2187, 0, 167, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 2202, 1, 67, 0, 2, 0 },
    { NULL, NULL, NULL, { 4560, -2243, -1765, 0 }, { { 2064, 0, -176, 0 }, { 2064, 0, 176, 0 }, { -2064, 0, -176, 0 }, { -2064, 0, 176, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 2063, 257, 67, 128, 2, 0 },
    { NULL, NULL, NULL, { 4574, -1638, -2432, 0 }, { { 2064, 0, -224, 0 }, { 2064, 0, 224, 0 }, { -2064, 0, -224, 0 }, { -2064, 0, 224, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2063, 1, 67, 0, 2, 0 },
    { NULL, NULL, NULL, { -4433, -1376, -4385, 0 }, { { -1197, 1024, 433, 0 }, { 1198, 1024, -433, 0 }, { -1197, -1024, 433, 0 }, { 1198, -1024, -433, 0 } }, { 1399, 0, 3869, 0 }, { 1567, 0, 3784, 0 }, 1634, 0x8000, 3, 52, 2, 0 },
    { NULL, NULL, NULL, { 4494, -1376, -4418, 0 }, { { -1141, 1024, -417, 0 }, { 1142, 1024, 418, 0 }, { -1141, -1024, -417, 0 }, { 1142, -1024, 418, 0 } }, { -1410, 0, 3848, 0 }, { -1257, 0, 3898, 0 }, 1588, 0x8000, 9, 68, 2, 0 },
    { NULL, NULL, NULL, { 52, -1680, -3974, 0 }, { { 1496, 0, -1411, 0 }, { 1110, 0, 439, 0 }, { -1444, 0, -1415, 0 }, { -1127, 0, 436, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 2048, 2, 4, 0, 4, 0 },
    { NULL, NULL, NULL, { 168, -2272, -80, 0 }, { { 888, 0, 192, 0 }, { 1112, 0, 1601, 0 }, { -839, 0, 192, 0 }, { -1159, 0, 1601, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, 4091, 0 }, 1974, 5, 0, 0, 4, 0 },
    { NULL, NULL, NULL, { 6272, -2328, 2400, 0 }, { { -912, 0, -736, 0 }, { 912, 0, -736, 0 }, { -912, 0, 736, 0 }, { 912, 0, 736, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { -4464, -1200, -4609, 0 }, { { 888, 0, -537, 0 }, { 1040, 0, -152, 0 }, { -1040, 0, 153, 0 }, { -888, 0, 538, 0 } }, { 0, 4103, 0, 0 }, { -1380, 0, -3857, 0 }, 1047, 0x8001, 67, 16, 2, 0 },
    { NULL, NULL, NULL, { 4689, -1152, -4801, 0 }, { { -1054, 0, -113, 0 }, { -858, 0, -621, 0 }, { 858, 0, 621, 0 }, { 1054, 0, 113, 0 } }, { 0, 4102, 0, 0 }, { 1567, 0, -3784, 0 }, 1055, 0x8001, 67, 240, 2, 0 },
    { NULL, NULL, NULL, { -3568, -2304, 3600, 0 }, { { 1680, 0, -1312, 0 }, { 1680, 0, 1473, 0 }, { -1679, 0, -1472, 0 }, { -1679, 0, 1313, 0 } }, { 0, 4108, 0, 0 }, { -201, 0, 4091, 0 }, 2231, 2, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { -1552, -2272, -448, 0 }, { { -1376, 0, -1088, 0 }, { 768, 0, -1088, 0 }, { -1120, 0, 1184, 0 }, { 768, 0, 1376, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1750, 2, 7, 0, 4, 0 },
    { NULL, NULL, NULL, { 1536, -2272, -640, 0 }, { { -1952, 0, -672, 0 }, { 1248, 0, -672, 0 }, { -1280, 0, 1568, 0 }, { 1248, 0, 1408, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 2063, 2, 7, 0, 4, 0 },
    { NULL, NULL, NULL, { -769, -1696, -6753, 0 }, { { -943, 0, 302, 0 }, { 1265, 0, -16, 0 }, { -560, 0, 1105, 0 }, { 1040, 0, 787, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1299, 2, 9, 0, 4, 0 },
    { NULL, NULL, NULL, { -5577, -2272, -1575, 0 }, { { -824, 0, -504, 0 }, { 1288, 0, -504, 0 }, { -856, 0, 1000, 0 }, { 1288, 0, 1480, 0 } }, { 0, 4103, 0, 0 }, { 4096, 0, 0, 0 }, 1958, 2, 7, 0, 4, 0 },
    { NULL, NULL, NULL, { -1104, -2304, 5136, 0 }, { { -992, 0, -848, 0 }, { 672, 0, -848, 0 }, { -992, 0, 80, 0 }, { 672, 0, 80, 0 } }, { 0, 4107, 0, 0 }, { 4017, 0, -799, 0 }, 1299, 6, 13, 0, 4, 0 },
    { NULL, NULL, NULL, { 1824, -2240, 3801, 0 }, { { 3504, 0, -2168, 0 }, { 3504, 0, 2249, 0 }, { -3503, 0, -1720, 0 }, { -3503, 0, 1641, 0 } }, { 0, 4102, 0, 0 }, { -201, 0, 4091, 0 }, 4159, 2, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { -5281, -1640, -2401, 0 }, { { -991, 0, -568, 0 }, { 623, 0, -972, 0 }, { -696, 0, 744, 0 }, { 1066, 0, 797, 0 } }, { 0, 4117, 0, 0 }, { 3513, 0, -2106, 0 }, 1330, 2, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -128, -1664, -3296, 0 }, { { 7440, 0, -176, 0 }, { 7440, 0, 176, 0 }, { -7440, 0, -176, 0 }, { -7440, 0, 176, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 7437, 0x8002, 14, 255, 3, 0 },
    { NULL, NULL, NULL, { 0, -1664, -4784, 0 }, { { 1248, 0, -640, 0 }, { 1248, 0, 641, 0 }, { -1247, 0, -640, 0 }, { -1247, 0, 641, 0 } }, { 0, 4102, 0, 0 }, { 201, 0, -4091, 0 }, 1402, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 192, -2252, -1664, 0 }, { { 7248, 0, -304, 0 }, { 7248, 0, 272, 0 }, { -7248, 0, -272, 0 }, { -7248, 0, 304, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 7240, 0x8002, 16, 255, 131, 0 },
};

GpAreaTmdRec D_acropolis_square_80185E38[2] = {
    { 19, 118, 0, 0, { 0, 0 }, D_8013A468 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_acropolis_square_80185E50[3] = {
    { NULL, NULL },
    { D_map_akropolis_8017ACBC, D_acropolis_square_80185E38 },
    { NULL, NULL },
};

GpPointLight D_acropolis_square_80185E68[16] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3005, -4865, 2030 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 2949, 2867, { 0, 0 } }, 10, 8250 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5380, -2055, -3345 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2375, 2293, 2211, { 0, 0 } }, 100, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5360, -2055, -3345 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2375, 2293, 2211, { 0, 0 } }, 100, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4395, -1475, -4140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1884, 1802, 1720, { 0, 0 } }, 100, 750 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4405, -1475, -4140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1884, 1802, 1720, { 0, 0 } }, 100, 750 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5104, -900, -5740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2539, { 0, 0 } }, 100, 1000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5037, -900, -5783 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2539, { 0, 0 } }, 100, 1000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6430, -4865, 5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, 650, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6445, -4865, 5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 3112, { 0, 0 } }, 650, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3108, -4865, 2030 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 2949, 2867, { 0, 0 } }, 10, 8250 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1960, -4265, -5722 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2949, 2949, 2867, { 0, 0 } }, 10, 7500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1970, -4265, -5722 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 3031, 2949, { 0, 0 } }, 10, 7500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4925, -1925, -2165 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2211, 2129, 2048, { 0, 0 } }, 100, 1250 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5090, -1925, -2165 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2211, 2129, 2048, { 0, 0 } }, 100, 1250 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2869, -1925, -2165 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2211, 2129, 2048, { 0, 0 } }, 100, 1250 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2805, -1925, -2165 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2211, 2129, 2048, { 0, 0 } }, 100, 1250 },
};

GpRoomCoordSet D_acropolis_square_80186468[1] = {
    { 0, NULL, 16, D_acropolis_square_80185E68, 0, NULL },
};

GpRoomBoundVec D_acropolis_square_80186480[16] = {
    { 15, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

GpSprtCmd D_acropolis_square_80186500[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_80186510[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_square_80186520[73] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -80, 2750, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 2750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 2750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 0, 2875, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 8, 2450, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 0, 2450, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 8, 2450, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 40, 1550, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, 32, 1575, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 32, 1600, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 32, 1625, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 24, 1650, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 24, 1675, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 16, 1700, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 16, 1725, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 40, 1750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 48, 625, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 48, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 56, 625, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 625, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 88, 625, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -80, 64, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 88, 625, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 88, 500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 56, 500, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, 64, 625, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -24, 88, 625, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 625, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 72, 625, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 625, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, 72, 625, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, 64, 625, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 120, 64, 625, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 64, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 104, 625, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 104, 625, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 56, 625, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 40, 1650, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 40, 1650, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 16, 1750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 16, 1725, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 32, 1700, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1675, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 2475, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 24, 2425, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -32, 24, 2325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, 32, 2312, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 32, 2312, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 24, 2375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 32, 2425, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 2500, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 16, 2500, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 16, 2450, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -16, 2450, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, 16, 2425, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 16, 2400, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -24, -16, 2425, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -24, 2400, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -16, 2425, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 16, 2425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 16, 2450, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -8, 2500, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -16, 2450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 16, 2500, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -24, 2450, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 2450, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -64, 2450, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -64, 2450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -112, 2450, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -88, 2450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -88, 2450, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -120, 2450, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 2450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_square_80186AD4[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 3, 0, 0, { 5, 0 } },
    { 7, 9, 0, 0, { 4, 0 } },
    { 16, 11, 0, 0, { 6, 0 } },
    { 27, 10, 0, 0, { 1, 0 } },
    { 37, 6, 0, 0, { 7, 0 } },
    { 43, 7, 0, 0, { 3, 0 } },
    { 50, 14, 0, 0, { 8, 0 } },
    { 64, 9, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_square_80186B2C[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 64, 1337, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 1325, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 1175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 64, 1180, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 48, 1180, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 64, 1212, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 40, 1212, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 40, 1225, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 64, 1225, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 32, 1337, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 32, 1325, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 24, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 64, 1375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 112, 875, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 875, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 875, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 96, 875, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -40, 88, 875, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 72, 875, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 875, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 88, 875, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 48, 1556, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 104, 1337, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 104, 1300, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 104, 1300, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1250, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 1325, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 1312, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 88, 1350, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 88, 1325, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 72, 1412, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 72, 1406, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 72, 1356, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 64, 1506, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1562, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 64, 1530, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 56, 1562, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 48, 1706, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 40, 1712, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_square_80186E60[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 9, 0, 0, { 2, 0 } },
    { 22, 19, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_square_80186E88[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 80, 1118, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 80, 1106, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 80, 1112, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 64, 1112, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 80, 1115, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 56, 1115, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 80, 1117, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, 48, 1117, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 40, 1118, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 40, 1120, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 80, 1120, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 1120, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 112, 937, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 72, 937, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, 88, 937, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 937, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 1275, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 104, 1200, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1200, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 104, 1200, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 104, 1200, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 88, 1300, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 88, 1300, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 1300, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 1300, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 1350, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 80, 1350, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 72, 1400, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1375, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 64, 1450, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1450, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_square_801870F4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 4, 0, 0, { 2, 0 } },
    { 16, 15, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_square_8018711C[91] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 1487, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 40, 1500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 72, 1450, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 40, 1450, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 1450, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 1487, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 72, 1487, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 1500, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1500, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 1500, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 48, 1350, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 1350, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 32, 1475, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 1500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1500, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 56, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 48, 1400, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 40, 1425, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 72, 1350, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -104, 56, 1362, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 56, 1362, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -40, 56, 1362, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 48, 1375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 1375, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 88, 1275, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1275, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 72, 1275, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 72, 1275, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 72, 1275, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 56, 1275, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -128, 64, 1275, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 1275, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 1275, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 104, 775, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -48, 104, 775, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 104, 775, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 104, 775, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 88, 800, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 88, 812, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 88, 812, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 88, 812, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 72, 862, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 72, 862, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 72, 862, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -48, 64, 875, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 64, 875, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 1225, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 72, 1212, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 1212, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 56, 1212, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 40, 1225, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 120, 48, 1225, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 48, 3000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 16, 56, 2875, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 48, 56, 2875, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, 56, 2875, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 56, 2875, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 56, 2875, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 40, 3000, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 40, 3000, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 40, 3000, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 40, 3000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 32, 3125, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 32, 3125, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 32, 3125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 24, 3250, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 3250, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 2762, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 2762, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, -48, 2762, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 0, 2762, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -40, 2762, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, -48, 2750, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, -48, 2762, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, -40, 2925, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 24, 2925, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, 0, 2925, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -40, 2925, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, -48, 2925, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 0, 2925, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, 88, 987, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 88, 987, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 88, 987, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 8, 72, 1000, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 72, 1000, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 72, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 56, 1062, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 56, 1062, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 64, 1062, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_square_80187838[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 6, 0 } },
    { 7, 8, 0, 0, { 1, 0 } },
    { 15, 3, 0, 0, { 8, 0 } },
    { 18, 7, 0, 0, { 0, 0 } },
    { 25, 9, 0, 0, { 5, 0 } },
    { 34, 14, 0, 0, { 2, 0 } },
    { 48, 6, 0, 0, { 9, 0 } },
    { 54, 15, 0, 0, { 4, 0 } },
    { 69, 13, 0, 0, { 7, 0 } },
    { 82, 9, 0, 0, { 3, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_square_80187898[94] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 24, 2050, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 2050, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -32, 2050, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 2050, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -72, 2050, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 8, 2050, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 136, -24, 2050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -48, 2050, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 2062, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 24, 2025, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 40, 1912, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 32, 2025, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 64, 2025, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 32, 2025, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 16, 2025, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 24, 2037, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 32, 2062, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 32, 2050, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 2050, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 16, 2050, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 32, 2037, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 64, 2037, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 32, 2025, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 64, 2025, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 64, 2025, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 64, 1200, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 1200, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 1200, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -56, 1200, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -96, 1200, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -104, 1200, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -88, 1200, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 40, 1987, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 56, 1987, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 8, 1987, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -24, 1987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -56, 1987, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -48, 1987, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 16, 2150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -16, 2150, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 2150, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 8, 2250, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 2250, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 2125, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 2125, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 2125, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 2125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 40, 2250, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 32, 2250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 48, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 2125, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 2125, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 40, 2125, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 2112, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 48, 2112, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 2112, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 40, 2112, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 32, 2112, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 2100, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 2100, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 40, 2100, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 1550, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 1550, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 1600, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 48, 1600, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 40, 1700, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1750, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 48, 1600, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1250, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 64, 1312, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 56, 1312, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -144, 72, 1300, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 72, 1300, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 1312, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 802, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 88, 780, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -104, 96, 775, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 80, 787, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -72, 80, 780, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -32, 80, 780, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 72, 795, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 72, 795, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 1950, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, 40, 1950, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1950, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, 72, 1300, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 120, 72, 1300, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 64, 1305, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 56, 1305, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 64, 1305, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 56, 1305, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_square_80187FF0[18] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 13, 0 } },
    { 8, 17, 0, 0, { 2, 0 } },
    { 25, 7, 0, 0, { 9, 0 } },
    { 32, 6, 0, 0, { 3, 0 } },
    { 38, 3, 0, 0, { 11, 0 } },
    { 41, 2, 0, 0, { 0, 0 } },
    { 43, 4, 0, 0, { 12, 0 } },
    { 47, 2, 0, 0, { 1, 0 } },
    { 49, 4, 0, 0, { 8, 0 } },
    { 53, 5, 0, 0, { 6, 0 } },
    { 58, 3, 0, 0, { 15, 0 } },
    { 61, 7, 0, 0, { 7, 0 } },
    { 68, 6, 0, 0, { 10, 0 } },
    { 74, 11, 0, 0, { 4, 0 } },
    { 85, 3, 0, 0, { 14, 0 } },
    { 88, 6, 0, 0, { 5, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_square_80188080[53] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 48, 1125, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 80, 1075, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 1125, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 80, 1075, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 1125, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 40, 1125, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 1075, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 1075, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 40, 1125, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 24, 2185, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 24, 2185, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 32, 2185, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 2185, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 32, 2185, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 24, 2225, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 24, 2225, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 16, 2225, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 16, 2225, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 1250, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 1250, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 64, 1400, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 1400, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 80, 1250, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 56, 1400, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 64, 1400, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 72, 1393, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 56, 1395, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 48, 1417, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1200, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 64, 1162, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 40, 1175, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 32, 1187, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, 32, 1200, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 40, 1350, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, 0, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -40, 1350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, -72, 1350, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -144, -96, 1350, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 2375, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 8, 2375, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -32, 2375, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, -56, 2375, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 2375, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -8, 8, 3375, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -24, 3375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, -40, 3375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 0, 3375, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 3375, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 16, 3375, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -16, 3375, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -40, 3375, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_square_801884A4[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 5, 0, 0, { 6, 0 } },
    { 14, 4, 0, 0, { 2, 0 } },
    { 18, 8, 0, 0, { 7, 0 } },
    { 26, 3, 0, 0, { 5, 0 } },
    { 29, 6, 0, 0, { 8, 0 } },
    { 35, 5, 0, 0, { 4, 0 } },
    { 40, 5, 0, 0, { 9, 0 } },
    { 45, 3, 0, 0, { 0, 0 } },
    { 48, 2, 0, 0, { 10, 0 } },
    { 50, 3, 0, 0, { 3, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018850C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018851C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018852C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018853C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018854C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018855C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_square_8018856C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_acropolis_square_8018857C[15] = {
    { { .empty = D_acropolis_square_80186500 }, D_acropolis_square_80186500, NULL },
    { { .empty = D_acropolis_square_80186510 }, D_acropolis_square_80186510, NULL },
    { { .elements = D_acropolis_square_80186520 }, D_acropolis_square_80186AD4, NULL },
    { { .elements = D_acropolis_square_80186B2C }, D_acropolis_square_80186E60, NULL },
    { { .elements = D_acropolis_square_80186E88 }, D_acropolis_square_801870F4, NULL },
    { { .elements = D_acropolis_square_8018711C }, D_acropolis_square_80187838, NULL },
    { { .elements = D_acropolis_square_80187898 }, D_acropolis_square_80187FF0, NULL },
    { { .elements = D_acropolis_square_80188080 }, D_acropolis_square_801884A4, NULL },
    { { .empty = D_acropolis_square_8018850C }, D_acropolis_square_8018850C, NULL },
    { { .empty = D_acropolis_square_8018851C }, D_acropolis_square_8018851C, NULL },
    { { .empty = D_acropolis_square_8018852C }, D_acropolis_square_8018852C, NULL },
    { { .empty = D_acropolis_square_8018853C }, D_acropolis_square_8018853C, NULL },
    { { .empty = D_acropolis_square_8018854C }, D_acropolis_square_8018854C, NULL },
    { { .empty = D_acropolis_square_8018855C }, D_acropolis_square_8018855C, NULL },
    { { .empty = D_acropolis_square_8018856C }, D_acropolis_square_8018856C, NULL },
};

GpViewRec D_acropolis_square_80188630[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x704E, 100 } }, 348 },
    { { { { 3936, 0, 1132 }, { 14, 4095, -48 }, { -1132, 50, 3936 } }, { -3740, 3950, 0x2FEE } }, 195 },
    { { { { 4080, 0, 358 }, { 94, 3950, -1079 }, { -345, 1083, 3934 } }, { -370, 4950, 0x2828 } }, 207 },
    { { { { 3737, 0, 1674 }, { 531, 3884, -1185 }, { -1588, 1299, 3544 } }, { 1990, 4870, 9170 } }, 263 },
    { { { { 3750, 0, -1647 }, { -542, 3867, -1235 }, { 1555, 1349, 3540 } }, { -1930, 4800, 8370 } }, 246 },
    { { { { -1334, 0, -3872 }, { -103, 4094, 35 }, { 3871, 109, -1334 } }, { 2600, 3765, -4090 } }, 246 },
    { { { { -920, 0, -3991 }, { 300, 4084, -69 }, { 3979, -308, -917 } }, { 6160, 3515, -3630 } }, 207 },
    { { { { 4041, 0, 667 }, { 0, 4095, -3 }, { -667, 3, 4041 } }, { 3090, 3725, 5820 } }, 235 },
    { { { { -872, 0, -4002 }, { -1083, 3942, 236 }, { 3852, 1109, -839 } }, { -5555, 4045, -2590 } }, 269 },
    { { { { -2704, 0, -3076 }, { -109, 4093, 96 }, { 3074, 145, -2702 } }, { 1780, 3230, -1710 } }, 230 },
    { { { { -4018, 0, -791 }, { -243, 3897, 1236 }, { 753, 1260, -3823 } }, { 130, 2975, -880 } }, 257 },
    { { { { 3944, 0, 1103 }, { 808, 2787, -2890 }, { -750, 3001, 2684 } }, { -1310, 3273, 5158 } }, 329 },
    { { { { -704, 0, 4035 }, { 189, 4091, 33 }, { -4030, 192, -703 } }, { 1800, 3413, -820 } }, 257 },
    { { { { 1064, 0, -3955 }, { -2268, 3355, -610 }, { 3239, 2349, 872 } }, { -4075, 5389, -1590 } }, 269 },
    { { { { 1034, 0, -3963 }, { 258, 4087, 67 }, { 3954, -266, 1031 } }, { 7815, 3515, 1095 } }, 418 },
};

s32 D_acropolis_square_8018884C[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_acropolis_square_80188858[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_acropolis_square_80188860[1] = {
    { 0, 0, 1, 0, D_acropolis_square_8018884C },
};

GpRoomParamRec * D_acropolis_square_80188868[8] = {
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188860,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
    D_acropolis_square_80188858,
};

GpAreaApplyRec D_acropolis_square_80188888[4] = {
    { 1, 3, 3, 1 },
    { 1, 4, 3, 1 },
    { 1, 19, 2, 1 },
    { 255, 0, 0, 0 },
};

s32 D_acropolis_square_80188898 = 0;

Task * D_acropolis_square_8018889C = NULL;

s32 D_acropolis_square_801888A0 = 0;

s32 D_acropolis_square_801888A4 = 0;

Task * D_acropolis_square_801888A8 = NULL;

AcropolisSquareStorage88AC D_acropolis_square_801888AC = { 0 };

GpCoord D_acropolis_square_801888CC = { 0 };

/// "Telephone", the title of the menu panel `func_acropolis_square_80180804`
/// runs. Two non-zero bytes follow its terminator, so it stays assembly.
static const char D_acropolis_square_8017D648[];

static void func_acropolis_square_8017D714(Task* task);
static void func_acropolis_square_80180034(UiList* list, UiObject* obj);
static void func_acropolis_square_80180330(UiList* list, UiObject* obj);
static void func_acropolis_square_80180B58(u8* str, s32 decimals);
static u8*  func_acropolis_square_80180BC8(u8* buf, s32 value, s32 decimals);
static void func_acropolis_square_80180DAC(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
static void func_acropolis_square_8018345C(void);

/// Sets up the room's mirror: re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block the reflection's coordinate frame and matrices live
/// in, and hangs the task off the player task so it dies with it.
/// `spawnArg1` must be 0 or 1, and 0 also raises `GameSession::field_4E`. The
/// two child tasks reflect the player's held-object tasks
/// (`GameActor::field_920` / `field_924`).
static void func_acropolis_square_8017D714(Task* task)
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
            spawned = Task_SpawnFromTable(D_acropolis_square_80183468, 1, i, task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_acropolis_square_8017D8C8(task);
}

/// Per-frame state of the mirror task `func_acropolis_square_8017D714` sets up.
///
/// When the player's equipped weapon changes it spawns reflection tasks for
/// the player's two held-object tasks. When the view moves it rebuilds the
/// reflection's coordinate frame: mirror 0 copies the view matrix with its
/// second row negated and applies location-specific corrections, any other
/// mirror reflects through a plane chosen by the current stage, area and view.
/// On the frame after mirror 0 rebuilds, it queues packets that copy the frame
/// buffer into the off-screen strip at x = `width`. In stages 1 and 5 it
/// projects the reflected body to find its screen rectangle and, where that
/// overlaps the mirror's clip rectangle, draws quads sampling that strip;
/// otherwise the reflection is hidden. Every frame it copies the player's pose
/// and light matrices onto the reflection.
static void func_acropolis_square_8017D8C8(Task* task)
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
                spawned = Task_SpawnFromTable(D_acropolis_square_80183468, 1, i + 2, task);
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
static const VECTOR D_acropolis_square_8017D5C4 = { -0x1000, 0x1000, 0x1000 };

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task the room set up and the parent is the held-object task being
/// reflected. On the first frame it clones the parent's TMD source, parents the clone's root coordinate to the
/// mirrored player's corresponding part, points the clone at the mirror's
/// light and color matrices and negates the X translation; every frame it
/// republishes the mirror model's draw flags onto the clone.
void func_acropolis_square_8017F24C(Task* task)
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
    mirrorPart  = &mirror->extra.tmd->coords[D_acropolis_square_80183464[task->spawnArg1.value]];
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
            scale = D_acropolis_square_8017D5C4;
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
void func_acropolis_square_8017F41C(Task* task)
{
    TaskFunc states[2] = {
        func_acropolis_square_8017D714,
        func_acropolis_square_8017D8C8,
    };

    states[task->state](task);
}

/// Draws one row of the play-data statistics panel: the row label, then the
/// statistic `arg0->field_8` selects - play time, several save counters, and
/// two percentages printed with two decimals and a "%" suffix. While the row is
/// selected its help text goes to the UI holder.
void func_acropolis_square_8017F46C(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_acropolis_square_80183504,
                D_acropolis_square_80183530,
                D_acropolis_square_80183554,
                D_acropolis_square_80183584,
                D_acropolis_square_801835B8,
                D_acropolis_square_801835EC,
                D_acropolis_square_80183624,
                D_acropolis_square_80183658,
                D_acropolis_square_80183690,
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
            Text_DrawString(&req, D_acropolis_square_801834A8);
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
            Text_DrawString(&req, D_acropolis_square_801834D8);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_acropolis_square_801834F8);
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
            Text_DrawString(&req, D_acropolis_square_801834B0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_acropolis_square_801834F8);
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
            Text_DrawString(&req, D_acropolis_square_801834B4);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_acropolis_square_801834F8);
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
            Text_DrawString(&req, D_acropolis_square_801834BC);
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
            Text_Strcat(p, D_acropolis_square_80183500);
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
            Text_DrawString(&req, D_acropolis_square_801834C8);
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
            Text_Strcat(p, D_acropolis_square_80183500);
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
            Text_DrawString(&req, D_acropolis_square_801834E0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_acropolis_square_801834F8);
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
            Text_DrawString(&req, D_acropolis_square_801834E8);
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
            Text_DrawString(&req, D_acropolis_square_801834F0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// "Play Data", the title of the play-data menu panel
/// `func_acropolis_square_80180CBC` draws.
static const char D_acropolis_square_8017D620[] = "Play Data";

/// Text drawn in place of a usage row's percentage once it reaches 100 percent.
static const u8 D_acropolis_square_8017D62C[] = "100.0%";

/// Draws one row of a play-data usage panel from the `RoomItemUsage` block at
/// the owner task's `work`: the item's name and icon, its share of all uses as
/// a two-decimal percentage, and a gouraud bar scaled by the row's
/// `barWidths`. While the row is selected the item is previewed, and a confirm
/// opens the item's detail panel.
void func_acropolis_square_8017FC38(UiList* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_acropolis_square_8017D62C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_acropolis_square_80183500);
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
static void func_acropolis_square_80180034(UiList* list, UiObject* obj)
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
static void func_acropolis_square_80180330(UiList* list, UiObject* obj)
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
static const char D_acropolis_square_8017D634[] = "Weapon Data";
static const char D_acropolis_square_8017D640[] = "PE Data";

/// Task of the usage panel: draws the weapon or Parasite Energy title
/// (`spawnArg1`), and on its first tick allocates the `RoomItemUsage` /
/// `RoomPeUsage` block, spawns the panel and fills its list. Cancel closes it,
/// and a child panel that closes hands control back.
void func_acropolis_square_80180650(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_acropolis_square_801836F4;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_acropolis_square_8017D634);
    } else {
        Ui_DrawText(&(obj)->panel, D_acropolis_square_8017D640);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_acropolis_square_80183718, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_acropolis_square_80180034(list, obj);
        } else {
            func_acropolis_square_80180330(list, obj);
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
static const char D_acropolis_square_8017D648[12] = "Telephone\0\xDC\xDD";

/// Task of the telephone menu panel. Until the save allows it (demo scene 1
/// or a clear) it only spawns the generic panel; otherwise it lays out its
/// list and draws the title. Choosing an entry opens the item prompt, and
/// cancel closes the panel.
void func_acropolis_square_80180804(Task* task)
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
    list          = &D_acropolis_square_8018377C;
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
        Ui_DrawText(&(obj)->panel, D_acropolis_square_8017D648);
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
void func_acropolis_square_80180AFC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_acropolis_square_801811EC;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void func_acropolis_square_80180B58(u8* str, s32 decimals)
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
static u8* func_acropolis_square_80180BC8(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_acropolis_square_80183500);
    return buf;
}

/// Task of the play-data menu panel: draws "Play Data", and on its first tick
/// spawns the panel and lays out its list. Cancel closes it.
void func_acropolis_square_80180CBC(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_acropolis_square_801836CC;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_acropolis_square_8017D620);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_acropolis_square_80183718, 0, 0, 1, obj);
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
static void func_acropolis_square_80180DAC(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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
void func_acropolis_square_80180EB0(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_80183480, prompt->field_1C, 1, 0);
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
void func_acropolis_square_80180F94(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_80183488, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_square_80183734, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Menu entry "Weapon Data": on a confirm, opens the usage panel for weapons
/// and moves the owner task to state 2.
void func_acropolis_square_8018105C(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_80183494, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_square_80183750, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Menu entry "PE Data": on a confirm, opens the usage panel for Parasite
/// Energy and moves the owner task to state 2.
void func_acropolis_square_80181124(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_801834A0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_square_80183750, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
static void func_acropolis_square_801811EC(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_acropolis_square_80181228(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_acropolis_square_801888A8 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694                  = Mc_SaveData[0].state.at4.loc.view;
                Mc_SaveData[0].state.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_acropolis_square_801888A8 = Task_SpawnFromTable(D_acropolis_square_801837A0, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_acropolis_square_801888A8);
                task->state++;
            } else if (Task_PollKill(D_acropolis_square_801888A8, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
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
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
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
            Mc_SaveData[0].state.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
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

/// State handlers of the room entry task `func_acropolis_square_80182308`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_acropolis_square_8017D6B4 = {
    {
        func_acropolis_square_80182260,
        func_acropolis_square_801822A4,
        taskKill,
    },
};

s32 func_acropolis_square_80181794(Task* task, s32 msgId, RoomEventMsg * arg2, RoomEventMsg * arg3)
{
    GpAreaKey key; // filled in but never used: the Gp_SetAreaObjId call the
                   // sibling rooms make with it is absent here
    u16 temp_s1;

    key.stage = 1;
    key.area  = 4;
    *arg3     = *arg2;
    if (arg2->prefix.packed == 9) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->field_5 == 0)) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 2);
        }
        if (arg2->prefix.packed == 9) {
            if (GameFlag_GetNibble(9) & 1) {
                arg3->field_3 = 2;
            }
        }
        return 1;
    }
    if (arg2->prefix.packed == 2) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->field_5 == 0)) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 2);
        }
        if (GameFlag_GetNibble(0) < 2) {
            return 1;
        }
        if ((GameFlag_GetNibble(0) == 2) || (GameFlag_GetNibble(0) >= 3)) {
            if (arg2->field_5 == 0) {
                do {
                    Gp_SetNibbleIf(arg2->field_6, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    if (arg2->prefix.packed == 0x11) {
        if (GameFlag_GetNibble(0) < 2) {
            return 1;
        }
        if ((GameFlag_GetNibble(0) == 2) || (GameFlag_GetNibble(0) >= 3)) {
            if (arg2->field_5 == 0) {
                do {
                    Gp_SetNibbleIf(arg2->field_6, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    temp_s1 = arg2->prefix.packed;
    if (temp_s1 == 3) {
        if (arg2->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    arg3->field_3 = 1;
                } else {
                    arg3->field_3 = 2;
                }
            } else {
                arg3->field_3 = temp_s1;
            }
        }
    }
    return 1;
}
s32 func_acropolis_square_801819BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 var_a0;

    if (arg2 == 2) {
        if (Mc_SaveData[0].state.at4.loc.warp == 7) {
            Mc_SaveData[0].state.at4.loc.warp = 1;
        }
        D_acropolis_square_801888AC.value.field_0  = 9;
        D_acropolis_square_801888AC.value.field_1  = 1;
        D_acropolis_square_801888AC.value.field_3  = 1;
        D_acropolis_square_801888AC.value.field_4  = 0x51010001;
        D_acropolis_square_801888AC.value.field_8  = 0x51010007;
        D_acropolis_square_801888AC.value.field_10 = 0x51010006;
        D_acropolis_square_801888AC.value.field_C  = 0x5101000B;
        D_acropolis_square_801888AC.value.field_2  = D_acropolis_square_8018382C;
        D_acropolis_square_8018382C          = 0;
        Task_SpawnFromTable(D_acropolis_square_801837A0, 0, 2, &D_acropolis_square_801888AC.value);
    }
    if ((arg2 == 0xE) && (GameFlag_GetNibble(0x124) == 0)) {
        GameFlag_SetNibble(0x124, 1);
        Gp_SpawnIfCapIdle(0xE, 1);
    }
    if ((arg2 == 0x10) && (GameFlag_GetNibble(0x156) == 0)) {
        GameFlag_SetNibble(0x156, 1);
        var_a0 = 0x11;
        if (Mc_SaveData[0].state.buttonLayout != 1) {
            var_a0 = 0x10;
        }
        Gp_SpawnIfCapIdle(var_a0, 1);
    }
    return 0;
}
/// Siren task for the square. States 0-2 arm the scene and tick, 3 fires the
/// first siren blast, 4 repeats it every 0x79 frames until the player answers,
/// and 5 waits for the scripted phase to advance before handing the scene off
/// to the slot-5 task and killing itself.
void func_acropolis_square_80181AEC(Task* task)
{
    s32 pan;
    s32 pan2;
    s32 pan3;
    s32 count;
    s32 count2;
    u32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            D_acropolis_square_8018382C = 1;
            D_acropolis_square_80188898 = 0;
            func_800E8634(D_acropolis_square_80183834, 0, D_acropolis_square_801838DC);
            goto advance;

        case 3:
            D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
            D_acropolis_square_801888CC.coord.t[1] = -0xF96;
            D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
            D_acropolis_square_801888CC.sub        = &gGfxViewCoord;
            Gp_UpdateCoord(&D_acropolis_square_801888CC);
            pan = Gp_GetObjPan(&D_acropolis_square_801888CC);
            SndEvt_EnqueueType6(
                0x51010009, (s8)pan, (s8)gpGetObjDepth(&D_acropolis_square_801888CC));
            goto advance;

        case 4:
            count                       = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count;
            if (count >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.sub        = &gGfxViewCoord;
                Gp_UpdateCoord(&D_acropolis_square_801888CC);
                pan2 = Gp_GetObjPan(&D_acropolis_square_801888CC);
                SndEvt_EnqueueType6(0x51010009, (s8)pan2,
                                    (s8)gpGetObjDepth(&D_acropolis_square_801888CC));
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            Gp_MsgPlayerWeapon(1);
            /* fallthrough */

        case 1:
        case 2:
        advance:
            task->state += 1;
            return;

        case 5:
            if ((u32)(Mc_SaveData[0].state.at4.loc.view - 5) >= 3U) {
                if (Mc_SaveData[0].state.at4.loc.view == 9) {
                    goto checkArmed;
                }
                goto handOff;
            }
        checkArmed:
            if (D_acropolis_square_8018382C == 0) {
            handOff:
                if (D_acropolis_square_8018382C != 0) {
                    GameFlag_SetNibble(3, 0);
                    GameFlag_SetNibble(0x155, 2);
                    D_acropolis_square_8018382C = 0;
                }
                Gp_DispatchMsg(gameGetPtrSlot(5), 0xC1F, 0, 0);
                SndEvt_EnqueueType7(0x51010009, 1);
                taskKill(task);
                return;
            }
            count2                      = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count2;
            if (count2 >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.sub        = &gGfxViewCoord;
                Gp_UpdateCoord(&D_acropolis_square_801888CC);
                pan3 = Gp_GetObjPan(&D_acropolis_square_801888CC);
                SndEvt_EnqueueType6(0x51010009, (s8)pan3,
                                    (s8)gpGetObjDepth(&D_acropolis_square_801888CC));
            }
            break;
    }
}
/// Scrolling backdrop task: three 256x240 sprite strips (the last one half
/// width) tiled across the screen from `D_acropolis_square_801888A0`, each with
/// its own texture page. States 0-3 slide the strip in and hold it for a while,
/// state 4 kills the task; every state still draws.
void func_acropolis_square_80181DD0(Task* task)
{
    SPRT*     p;
    DR_TPAGE* dr;
    s32       x;
    s32       i;
    s32       tpageX;
    s32       count;
    s32       count2;
    s32       pos;

    switch (task->state) {
        case 0:
            D_acropolis_square_801888A0 = -0x140;
            D_acropolis_square_801888A4 = 0;
            task->state                += 1;
            break;

        case 1:
            count                       = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count;
            if (count >= 0x2E) {
                task->state += 1;
            }
            break;

        case 2:
            pos                         = D_acropolis_square_801888A0 + 1;
            D_acropolis_square_801888A0 = pos;
            if (pos >= 0) {
                D_acropolis_square_801888A4 = 0;
                task->state                += 1;
            }
            break;

        case 3:
            count2                      = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count2;
            if (count2 >= 0x1F) {
                task->state += 1;
            }
            break;

        case 4:
            Mc_SaveData[0].state.at4.loc.view = 0xD;
            taskKill(task);
            break;
    }

    x = D_acropolis_square_801888A0;
    for (i = 0; i < 3; i++) {
        tpageX         = 0x1C0 + i * 0x80;
        p              = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setlen(p, 4);
        setcode(p, 0x65);
        p->x0 = x - 0xA0;
        p->y0 = -0x78;
        p->u0 = 0;
        p->v0 = 0;
        if (i == 2) {
            p->w = 0x80;
            p->h = 0xF0;
        } else {
            p->w = 0x100;
            p->h = 0xF0;
        }
        p->clut = GetClut(0, 0xFF);
        addPrim(&gGpuCurrentOt[4], p);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, GetTPage(1, 0, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[4], dr);

        x += 0x100;
    }
}

/// Sound task: plays the sound event `spawnArg2` on its first tick and again
/// at tick 0x50, then kills itself at tick 0x78.
void func_acropolis_square_80182048(Task* task)
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

s32 func_acropolis_square_801820D8(Task* task, s32 msgId, GpMsg13EF * arg2, GpMessageArg arg3)
{
    if (arg2->field_2 == 0) {
        Gp_SpawnIfCapIdle(5, 0);
    }
    return 0;
}

/// Always returns 0.
s32 func_acropolis_square_80182108(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_acropolis_square_80182110(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    SndEvt_EnqueueType6(D_acropolis_square_80183B34[arg2], 0, 0);
    return 0;
}

void func_acropolis_square_80182148(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(5);
            task->state++;
            return;
        case 1:
            Mc_SaveData[0].state.at4.loc.view = 7;
            task->state++;
            return;
        case 3:
            Gp_RunCapCmd1(5);
            task->state++;
            return;
        case 4:
        case 5:
            task->state++;
            return;
        case 6:
            Gp_RunCapCmd1(5);
            Mc_SaveData[0].state.at4.loc.view = 8;
            task->state++;
            return;
        case 2:
        case 7:
            GameFlag_SetNibble(0x15, 1);
            taskKill(task);
            return;
    }
}

void func_acropolis_square_80182200(s32 arg0)
{
    switch (arg0) { /* irregular */
        case 0:
            D_acropolis_square_8018889C = Task_SpawnFromTable(D_acropolis_square_80183808, 2, 0, 0);
            return;
        case 1:
            taskKill(D_acropolis_square_8018889C);
            return;
    }
}

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
static void func_acropolis_square_80182260(Task* task)
{
    task->msgTable = D_acropolis_square_801837C4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

static void func_acropolis_square_801822A4(Task* task)
{
    char pad[0x10];

    if (Mc_SaveData[0].state.at4.loc.warp == 7 && D_acropolis_square_80183830 == 0) {
        D_acropolis_square_80183830 = 1;
        Mc_SaveData[0].state.sceneEvent   = 2;
        func_800E8634(D_acropolis_square_8018399C, 0, D_acropolis_square_80183A5C);
    }
}

/// Room entry task: runs the state handler `D_acropolis_square_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_acropolis_square_80182308(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_square_8017D6B4;
    sp.funcs[task->state](task);
}

s32 func_acropolis_square_80182360(s32 unused)
{
    GpAreaKey key;

    if (GameFlag_GetNibble(0x1F) == 0) {
        GameFlag_SetNibble(0x1F, 1);
        key.stage = 1;
        key.area  = 1;
        Gp_SetAreaObjId(&key, 2, 1);
        gGameSession->eventState = 1;
        Task_SpawnFromTable(D_acropolis_square_80183808, 0, 0, 0);
        return 0;
    }
    return 1;
}

void func_acropolis_square_801823DC(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    coord = task->extra.tmd->coords;
    work  = task->spawnArg2.pointer;
    switch (task->state) { /* irregular */
        case 0:
            task->msgTable = D_acropolis_square_80183B58;
            Game_SetPtrSlot(task, 5);
            D_acropolis_square_80183B98 = 0;
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            return;
        case 1:
            if ((0x268 >> ((u8)gGameSession->at4.loc.view - 1)) & 1) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8DE;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x10000218 + 0x10E08,
                            &work->move);
            }
            if ((u8)gGameSession->at4.loc.view == 0xE) {
                work->move.vx = 0x18D2;
                work->move.vy = -0x100B;
                work->move.vz = 0x8AB;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x218 + 0x10010608,
                            &work->move);
            }
            if ((u8)gGameSession->at4.loc.view == 9) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8E8;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x118 + 0x80010308,
                            &work->move);
            }
            return;
    }
}

void func_acropolis_square_801825DC(Task* task)
{
    RoomGlowScratch* blk;
    POLY_G4*         prim;
    LINE_G3*         line;
    GpCoord*         coord;
    void*            mem;
    s32              i;
    s32              pulse;
    s32              level;
    s32              height;
    s16              amp;
    s16              flip;
    s32              ampSi;
    s32              ampHalf;
    u8               red;
    u8               cyan;
    s32              z;
    s32              shift;
    u32              depth;
    u32              tag;
    u_long*          ot;

    coord = task->extra.tmd->coords;
    mem   = task->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    blk         = SCRATCH_PUSH(RoomGlowScratch);
    blk->vec.vx = (u16)coord->workm.t[0];
    blk->vec.vy = (u16)coord->workm.t[1];
    blk->vec.vz = (u16)coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->vec);
    gte_rtps();
    gte_stsxy(&blk->sx);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        pulse  = gDisplayState.animFrame;
        pulse *= task->spawnArg1.value & 0xFF;
        flip   = (task->spawnArg1.value >> 16) & 1;
        if (pulse & 0x80) {
            level = ~pulse & 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        amp   = level * 2;
        level = task->spawnArg1.value;
        if (level < 0) {
            height      = (level >> 8) & 0xFF;
            blk->rOuter = (height * 0x600) / blk->otz;
            blk->rInner = (height * 0xC0) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                ampSi          = amp;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (ampSi * (flip ^ 1)) >> 1, (flip * ampSi) >> 1, (flip * ampSi) >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 12);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 2]) >> 12);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, ampSi * (flip ^ 1), flip * ampSi, flip * ampSi);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 2]) >> 13);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            ampHalf = amp >> 1;
            for (i = 2; i < 0x10; i += 8) {
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    red  = ampHalf * (flip ^ 1);
                    cyan = flip * ampHalf;
                    setRGB2(prim, red, cyan, cyan);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i]) >> 12);
                    prim->y0 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i - 4]) >> 12);
                    prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 11);
                    prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 11);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 8]) >> 12);
                    prim->y3 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i + 4]) >> 12);
                    shift    = gDisplayState.otDepthShift;
                    depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                    __asm__("" : "+r"(depth) : "r"(shift), "m"(gDisplayState.otDepthShift));
                    setaddr(prim, getaddr(((u_long*)((depth) + (uintptr)gGpuCurrentOt))));
                    ot  = ((u_long*)((((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt));
                    tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                    *ot = tag;
                    z   = blk->otz;
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG_USE(z, tag);
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, cyan, cyan);
                } while (0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 8]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 0xC]) >> 13);
                prim->y3 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i + 8]) >> 13);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        prim);
                z = blk->otz;
                __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_square_80183B68[i]));
                __asm__("" : "+r"(prim) : "r"(z), "r"(cyan));
                Gp_AddTpageShift((P_TAG*)prim, 1, z);
            }
        } else {
            blk->rOuter = (((level >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, amp * (flip ^ 1), flip * amp, flip * amp);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->rOuter;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->rOuter;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->rOuter) + blk->rOuter * (i + i);
                addPrim(((u_long*)((((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (task->spawnArg1.value & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)gGpuPrimCursor;
                    gGpuPrimCursor = line + 1;
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, amp * (flip ^ 1), flip * amp, flip * amp);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->rOuter * (i * 3 - 1);
                    line->y0 = blk->sy - blk->rOuter * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->rOuter * (i * 3 - 1);
                    line->y2 = blk->sy + blk->rOuter * (i + 1);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            line);
                    Gp_AddTpageShift((P_TAG*)line, 1, blk->otz);
                }
            }
        }
    }
    SCRATCH_POP(RoomGlowScratch);
    Gp_ReleaseState1CMem(mem, task);
}

s32 func_acropolis_square_8018344C(s32 arg0, s32 arg1, s32 arg2)
{
    D_acropolis_square_80183B98 = arg2;
    return 0;
}

static void func_acropolis_square_8018345C(void)
{
}
