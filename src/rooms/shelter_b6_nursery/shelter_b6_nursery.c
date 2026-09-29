#include "rooms/shelter_b6_nursery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern SVECTOR D_shelter_b6_nursery_801852F4[2];

/// Settings of the room's effect task `func_shelter_b6_nursery_801800A0`,
/// written together by `func_shelter_b6_nursery_80182D14`. A non-zero
/// `field_0` widens the glints of views 3, 6, 8 and 10; a non-zero `field_2`
/// fires a burst of sixteen effects in view 13, scaled by it, after which both
/// are cleared.
typedef struct ShelterB6NurseryPair {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} ShelterB6NurseryPair;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryPair, 0x4);

/// Scratch block one triangle is built in: its three corners in world space,
/// then the GTE depth and flag of its projection.
typedef struct _ShelterB6NurseryTriScratch {
    SVECTOR v[3];
    s32     otz;
    s32     flag;
} _ShelterB6NurseryTriScratch;

s32 rsin(s32);
s32 rcos(s32);

extern void func_80131E2C(void);
extern void func_80132000(void);
extern void func_80132028(void);

extern UiObjectDesc D_800611E4;

extern s32 D_80139964;
extern s32 D_8013A33C;
extern s32 D_8013A84C;
// Script in the companion actor slot; this address also holds a task table
// when a different actor package is loaded.
extern GpEvsCmd D_nursery_script_8013A8DC[];
extern s32      D_8013AF8C;
extern s32      D_8013BA84;

/// `Mc_SaveData[0].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.

/// View saved when the cutscene starts and restored when it ends.

/// Row labels of the play-data statistics list, one per row.
extern u8 D_shelter_b6_nursery_80184CE4[];
extern u8 D_shelter_b6_nursery_80184D14[];
extern u8 D_shelter_b6_nursery_80184CEC[];
extern u8 D_shelter_b6_nursery_80184CF0[];
extern u8 D_shelter_b6_nursery_80184CF8[];
extern u8 D_shelter_b6_nursery_80184D04[];
extern u8 D_shelter_b6_nursery_80184D1C[];
extern u8 D_shelter_b6_nursery_80184D24[];
extern u8 D_shelter_b6_nursery_80184D2C[];

/// Suffix appended to the plain counts.
extern u8 D_shelter_b6_nursery_80184D34[];

/// Suffix appended to the percentages.
extern u8 D_shelter_b6_nursery_80184D3C[];

/// Help lines shown while the matching row is selected.
extern u8 D_shelter_b6_nursery_80184D40[];
extern u8 D_shelter_b6_nursery_80184D6C[];
extern u8 D_shelter_b6_nursery_80184D90[];
extern u8 D_shelter_b6_nursery_80184DC0[];
extern u8 D_shelter_b6_nursery_80184DF4[];
extern u8 D_shelter_b6_nursery_80184E28[];
extern u8 D_shelter_b6_nursery_80184E60[];
extern u8 D_shelter_b6_nursery_80184E94[];
extern u8 D_shelter_b6_nursery_80184ECC[];

/// Lines of the four confirm prompts, and the panels two of them open.
extern u8           D_shelter_b6_nursery_80184CBC[];
extern u8           D_shelter_b6_nursery_80184CC4[];
extern UiObjectDesc D_shelter_b6_nursery_80184F70;
extern u8           D_shelter_b6_nursery_80184CD0[];
extern u8           D_shelter_b6_nursery_80184CDC[];
extern UiObjectDesc D_shelter_b6_nursery_80184F8C;

/// The play-data menu's list, the usage list, the child-panel descriptor both
/// spawn, and the telephone menu's list.
extern UiList       D_shelter_b6_nursery_80184F08;
extern UiList       D_shelter_b6_nursery_80184F30;
extern UiObjectDesc D_shelter_b6_nursery_80184F54;
extern UiList       D_shelter_b6_nursery_80184FB8;

/// Title of the telephone menu. The bytes after its terminator are not zero,
/// so it stays assembly.
static const char D_shelter_b6_nursery_8017D638[];

/// Task tables: the cutscene and its sound task; the ambient sound task.
extern TaskDesc D_shelter_b6_nursery_80184FDC[];
extern TaskDesc D_shelter_b6_nursery_80185000;

/// The room's message table.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpMsg13EF*, s32);
        s32 (*call2)(Task*, s32, GpSaveLoc*, GpSaveLoc*);
        s32 (*call3)(Task*, s32, s32, s32);
    } handler;
} ShelterB6NurseryMessageEntry;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryMessageEntry, 8);

extern ShelterB6NurseryMessageEntry D_shelter_b6_nursery_8018500C[5];

/// Per-view depth override for the ambient sound (-1 keeps the computed one).
extern s8 D_shelter_b6_nursery_80185034[];

/// Anchor points of the room's glints and effects.

/// The two ends of the trail effect, relative to its parent coordinate.

/// The cutscene's running sound task.
extern Task* D_shelter_b6_nursery_80187978;

/// Non-zero while the ambient sound task runs.
extern s32 D_shelter_b6_nursery_8018797C;

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomCutsceneRec value;
    u8              retained[8];
} ShelterB6NurseryStorage7980;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryStorage7980, 32);

extern ShelterB6NurseryStorage7980 D_shelter_b6_nursery_80187980;

/// Position the ambient sound is panned and attenuated from.
extern GpCoord D_shelter_b6_nursery_801879A0;

extern ShelterB6NurseryPair D_shelter_b6_nursery_801879F0;

static void func_shelter_b6_nursery_8017F4AC(Task* task);
static void func_shelter_b6_nursery_8017FEC4(Task* task);
static void func_shelter_b6_nursery_8017FF8C(Task* task);
static void func_shelter_b6_nursery_80180518(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b6_nursery_8018098C(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b6_nursery_80181EDC(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b6_nursery_80182330(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b6_nursery_801829E4(GpCoord* coord, s16 scale, s16 shade);
static void func_shelter_b6_nursery_80182FCC(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b6_nursery_801833F8(GpCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_b6_nursery_80183C7C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b6_nursery_801842FC(GpCoord* arg0, s16 arg1, u8* arg2);

void func_shelter_b6_nursery_8017D72C(UiList*, UiObject*);
void func_shelter_b6_nursery_8017DEF8(UiList*, UiObject*);
void func_shelter_b6_nursery_8017E910(Task*);
void func_shelter_b6_nursery_8017EDBC(Task*);
void func_shelter_b6_nursery_8017EF7C(Task*);
void func_shelter_b6_nursery_8017F170(UiList*, UiObject*);
void func_shelter_b6_nursery_8017F254(UiList*, UiObject*);
void func_shelter_b6_nursery_8017F31C(UiList*, UiObject*);
void func_shelter_b6_nursery_8017F3E4(UiList*, UiObject*);
void func_shelter_b6_nursery_8017F4E8(Task*);

void func_shelter_b6_nursery_8017F4E8(Task*);
void func_shelter_b6_nursery_8017FD3C(Task*);

void func_shelter_b6_nursery_8017FBC0(Task*);

u8 D_shelter_b6_nursery_80184CBC[8] = {
    83, 97, 118, 101, 0, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184CC4[12] = {
    80, 108, 97, 121, 32, 68, 97, 116, 97, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184CD0[12] = {
    87, 101, 97, 112, 111, 110, 32, 68, 97, 116, 97, 0,
};

u8 D_shelter_b6_nursery_80184CDC[8] = {
    80, 69, 32, 68, 97, 116, 97, 0,
};

u8 D_shelter_b6_nursery_80184CE4[8] = {
    84, 105, 109, 101, 0, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184CEC[4] = {
    87, 111, 110, 0,
};

u8 D_shelter_b6_nursery_80184CF0[8] = {
    69, 115, 99, 97, 112, 101, 100, 0,
};

u8 D_shelter_b6_nursery_80184CF8[12] = {
    66, 97, 116, 116, 108, 101, 115, 32, 119, 111, 110, 0,
};

u8 D_shelter_b6_nursery_80184D04[16] = {
    69, 120, 116, 101, 114, 109, 105, 110, 97, 116, 101, 100, 0, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184D14[8] = {
    83, 97, 118, 101, 100, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184D1C[8] = {
    67, 108, 101, 97, 114, 101, 100, 0,
};

u8 D_shelter_b6_nursery_80184D24[8] = {
    77, 97, 120, 32, 69, 88, 80, 0,
};

u8 D_shelter_b6_nursery_80184D2C[8] = {
    77, 97, 120, 32, 66, 80, 0, 0,
};

u8 D_shelter_b6_nursery_80184D34[8] = {
    32, 116, 105, 109, 101, 115, 0, 0,
};

u8 D_shelter_b6_nursery_80184D3C[4] = {
    37, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184D40[44] = {
    84, 111, 116, 97, 108, 32, 97, 109, 111, 117, 110, 116, 32, 111, 102, 10,
    116, 105, 109, 101, 32, 115, 112, 101, 110, 116, 32, 102, 111, 114, 32, 116,
    104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184D6C[36] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 115, 97, 118, 101, 115, 10,
    117, 115, 101, 100, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_shelter_b6_nursery_80184D90[48] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 110, 101, 109, 105, 101, 115, 10, 100, 101, 102, 101, 97, 116, 101, 100,
    32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101, 46, 0, 0,
};

u8 D_shelter_b6_nursery_80184DC0[52] = {
    84, 111, 116, 97, 108, 32, 110, 117, 109, 98, 101, 114, 32, 111, 102, 32,
    101, 115, 99, 97, 112, 101, 115, 10, 102, 114, 111, 109, 32, 98, 97, 116,
    116, 108, 101, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109, 101,
    46, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184DF4[52] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 98, 97, 116, 116, 108, 101, 115,
    32, 119, 111, 110, 32, 105, 110, 32, 116, 104, 105, 115, 32, 103, 97, 109,
    101, 46, 0, 0,
};

u8 D_shelter_b6_nursery_80184E28[56] = {
    67, 117, 114, 114, 101, 110, 116, 32, 112, 101, 114, 99, 101, 110, 116, 32,
    111, 102, 32, 116, 111, 116, 97, 108, 10, 101, 110, 101, 109, 105, 101, 115,
    32, 100, 101, 102, 101, 97, 116, 101, 100, 32, 105, 110, 32, 116, 104, 105,
    115, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_shelter_b6_nursery_80184E60[52] = {
    78, 117, 109, 98, 101, 114, 32, 111, 102, 32, 116, 105, 109, 101, 115, 32,
    121, 111, 117, 32, 104, 97, 118, 101, 10, 99, 108, 101, 97, 114, 101, 100,
    32, 116, 104, 101, 32, 103, 97, 109, 101, 32, 115, 111, 32, 102, 97, 114,
    46, 0, 0, 0,
};

u8 D_shelter_b6_nursery_80184E94[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 69, 88, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10,
    98, 121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104,
    101, 32, 103, 97, 109, 101, 46, 0,
};

u8 D_shelter_b6_nursery_80184ECC[56] = {
    71, 114, 101, 97, 116, 101, 115, 116, 32, 97, 109, 111, 117, 110, 116, 32,
    111, 102, 32, 66, 80, 32, 103, 97, 116, 104, 101, 114, 101, 100, 10, 98,
    121, 32, 116, 104, 101, 32, 101, 110, 100, 32, 111, 102, 32, 116, 104, 101,
    32, 103, 97, 109, 101, 46, 0, 0,
};

UiListItemFunc D_shelter_b6_nursery_80184F04[1] = {
    func_shelter_b6_nursery_8017D72C,
};

UiList D_shelter_b6_nursery_80184F08 = { D_shelter_b6_nursery_80184F04, 9, { .u = 9 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiListItemFunc D_shelter_b6_nursery_80184F2C[1] = {
    func_shelter_b6_nursery_8017DEF8,
};

UiList D_shelter_b6_nursery_80184F30 = { D_shelter_b6_nursery_80184F2C, 1, { .u = 1 }, 0, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

UiObjectDesc D_shelter_b6_nursery_80184F54 = { 3, 0xFF70, 64, 288, 40, 56, 0, 0, 192, func_shelter_b6_nursery_8017EDBC, 0 };

UiObjectDesc D_shelter_b6_nursery_80184F70 = { 2, 0xFF70, 0xFF98, 288, 120, 40, 0, 0, 192, func_shelter_b6_nursery_8017EF7C, 0 };

UiObjectDesc D_shelter_b6_nursery_80184F8C = { 2, 0xFF70, 0xFF98, 288, 168, 40, 0, 0, 192, func_shelter_b6_nursery_8017E910, 0 };

UiListItemFunc D_shelter_b6_nursery_80184FA8[4] = {
    func_shelter_b6_nursery_8017F170,
    func_shelter_b6_nursery_8017F254,
    func_shelter_b6_nursery_8017F31C,
    func_shelter_b6_nursery_8017F3E4,
};

UiList D_shelter_b6_nursery_80184FB8 = { D_shelter_b6_nursery_80184FA8, 4, { .u = 4 }, 1, 15, 0, { .u = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .u = 0 }, 0 };

void func_shelter_b6_nursery_8017F4E8(Task*);
void func_shelter_b6_nursery_8017FD3C(Task*);

TaskDesc D_shelter_b6_nursery_80184FDC[3] = {
    { 0, 32, func_shelter_b6_nursery_8017F4E8, { .model = NULL } },
    { 0, 32, func_shelter_b6_nursery_8017FD3C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

TaskDesc D_shelter_b6_nursery_80185000 = { 0, 32, func_shelter_b6_nursery_8017FBC0, { .model = NULL } };

s32 func_shelter_b6_nursery_8017FA54(Task*, s32, s32, s32);
s32 func_shelter_b6_nursery_8017FDCC(void);
s32 func_shelter_b6_nursery_8017FDD4(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32 func_shelter_b6_nursery_8017FE3C(Task*, s32, GpMsg13EF*, s32);

ShelterB6NurseryMessageEntry D_shelter_b6_nursery_8018500C[5] = {
    { 5102, { .call2 = func_shelter_b6_nursery_8017FDD4 } },
    { 5105, { .call0 = func_shelter_b6_nursery_8017FDCC } },
    { 5103, { .call1 = func_shelter_b6_nursery_8017FE3C } },
    { 5104, { .call3 = func_shelter_b6_nursery_8017FA54 } },
    { 2147483647, { .call0 = NULL } },
};

s8 D_shelter_b6_nursery_80185034[24] = {
    -1,
    -1,
    25,
    0,
    25,
    64,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    0,
    0,
};

SVECTOR D_shelter_b6_nursery_8018504C[7] = {
    { 6170, -80, -860, 0 },
    { 3000, -1000, 0, 0 },
    { 5500, 0, 2500, 0 },
    { 6000, 0, 0, 0 },
    { 4000, 0, -1000, 0 },
    { 2000, 0, 0, 0 },
    { 7000, -1500, 2000, 0 },
};

TmdBone D_shelter_b6_nursery_80185084[1] = {
#include "assets/shelter_b6_nursery_model_07D10_skeleton.inc"
};

u32 D_shelter_b6_nursery_801850A8[1] = {
#include "assets/shelter_b6_nursery_model_07D10_partVerts.inc"
};

SVECTOR D_shelter_b6_nursery_801850AC[12] = {
#include "assets/shelter_b6_nursery_model_07D10_verts.inc"
};

SVECTOR D_shelter_b6_nursery_8018510C[12] = {
#include "assets/shelter_b6_nursery_model_07D10_normals.inc"
};

u32 D_shelter_b6_nursery_8018516C[89] = {
#include "assets/shelter_b6_nursery_model_07D10_stream.inc"
};

TmdSource D_shelter_b6_nursery_801852D0 = {
    0, 576, 0, 1,
    D_shelter_b6_nursery_801850A8, D_shelter_b6_nursery_801850AC, D_shelter_b6_nursery_8018510C, D_shelter_b6_nursery_80185084, D_shelter_b6_nursery_8018516C,
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_shelter_b6_nursery_801852F4[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

u8 * D_shelter_b6_nursery_80185304[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b6_nursery_80185308[1] = {
    { { .bytes = { 19, 0 } } },
};

GpWarpRec D_shelter_b6_nursery_8018530C[2] = {
    { { .words = { 1024, 680, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 1024, 680, 0, 0 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 3072, 0x4CF4, 0, 3600 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x4CF4, 0, 3600 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 0, 0 },
};

SVECTOR D_shelter_b6_nursery_8018537C[14] = {
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 2896, 0, -2896, 0 },
    { 2896, 0, -2896, 0 },
    { 2896, 0, 2896, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 0, 4096, 0, 0 },
    { -2032, 0, 3556, 0 },
    { -2032, 0, -3556, 0 },
    { -2970, 0, 2821, 0 },
    { 3445, 0, -2215, 0 },
    { -2559, 0, -3198, 0 },
};

SVECTOR D_shelter_b6_nursery_801853EC[54] = {
    { 6950, -3000, 6450, 0 },
    { 6950, 0, 6450, 0 },
    { 4050, 0, 6450, 0 },
    { 4050, -3000, 6450, 0 },
    { 4050, 0, 1700, 0 },
    { 4050, -3000, 1700, 0 },
    { 3800, 0, 1450, 0 },
    { 3800, -3000, 1450, 0 },
    { 400, 0, 1450, 0 },
    { 400, -3000, 1450, 0 },
    { 50, 0, 1100, 0 },
    { 50, -3000, 1100, 0 },
    { 50, 0, -1100, 0 },
    { 50, -3000, -1100, 0 },
    { 400, 0, -1450, 0 },
    { 400, -3000, -1450, 0 },
    { 6950, 0, -1450, 0 },
    { 6950, -3000, -1450, 0 },
    { 5750, -1000, 2900, 0 },
    { 5750, -1000, 1900, 0 },
    { 5400, -1000, 2100, 0 },
    { 5400, -1000, 2700, 0 },
    { 6950, 0, 3350, 0 },
    { 6950, -1000, 3350, 0 },
    { 5750, -1000, 3350, 0 },
    { 5750, 0, 3350, 0 },
    { 5750, 0, 2900, 0 },
    { 5400, 0, 2700, 0 },
    { 5400, 0, 2100, 0 },
    { 5750, 0, 1900, 0 },
    { 5750, -1000, 1450, 0 },
    { 5750, 0, 1450, 0 },
    { 6250, -1000, 1450, 0 },
    { 6250, 0, 1450, 0 },
    { 6250, -1000, -1450, 0 },
    { 6250, 0, -1450, 0 },
    { 6950, -1000, -1450, 0 },
    { 6950, -1800, 4350, 0 },
    { 6950, -1800, 3350, 0 },
    { 6000, -1800, 3350, 0 },
    { 6950, 0, 4350, 0 },
    { 6000, 0, 3350, 0 },
    { 4850, -1700, 5200, 0 },
    { 4050, -1700, 6450, 0 },
    { 5300, -1700, 5900, 0 },
    { 6200, -1700, 5900, 0 },
    { 6950, -1700, 6450, 0 },
    { 6950, -1700, 5300, 0 },
    { 4050, 0, 5200, 0 },
    { 4050, -1700, 5200, 0 },
    { 4850, 0, 5200, 0 },
    { 5300, 0, 5900, 0 },
    { 6200, 0, 5900, 0 },
    { 6950, 0, 5300, 0 },
};

GpGridFace D_shelter_b6_nursery_8018559C[43] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 8, 7, 9 }, 0, 0 },
    { { 8, 10, 9, 11 }, 3, 0 },
    { { 10, 12, 11, 13 }, 1, 0 },
    { { 12, 14, 13, 15 }, 4, 0 },
    { { 14, 16, 15, 17 }, 5, 0 },
    { { 16, 1, 17, 0 }, 6, 0 },
    { { 14, 12, 8, 10 }, 7, 1 },
    { { 9, 11, 15, 13 }, 8, 0 },
    { { 17, 0, 5, 3 }, 8, 0 },
    { { 7, 17, 5, 0xFFFF }, 8, 0 },
    { { 4, 2, 16, 1 }, 7, 1 },
    { { 16, 6, 4, 0xFFFF }, 7, 1 },
    { { 15, 17, 9, 7 }, 8, 0 },
    { { 8, 6, 14, 16 }, 7, 1 },
    { { 19, 20, 18, 21 }, 7, 0 },
    { { 23, 24, 22, 25 }, 5, 0 },
    { { 24, 18, 25, 26 }, 6, 0 },
    { { 18, 21, 26, 27 }, 9, 0 },
    { { 21, 20, 27, 28 }, 6, 0 },
    { { 20, 19, 28, 29 }, 10, 0 },
    { { 19, 30, 29, 31 }, 6, 0 },
    { { 30, 32, 31, 33 }, 0, 0 },
    { { 32, 34, 33, 35 }, 6, 0 },
    { { 34, 36, 35, 16 }, 0, 0 },
    { { 23, 34, 32, 0xFFFF }, 7, 0 },
    { { 19, 32, 30, 0xFFFF }, 7, 0 },
    { { 23, 18, 24, 0xFFFF }, 7, 0 },
    { { 32, 19, 23, 18 }, 7, 0 },
    { { 34, 23, 36, 0xFFFF }, 7, 0 },
    { { 37, 38, 39, 0xFFFF }, 7, 0 },
    { { 37, 39, 40, 41 }, 11, 0 },
    { { 39, 38, 41, 22 }, 0, 0 },
    { { 42, 43, 44, 0xFFFF }, 7, 0 },
    { { 45, 46, 47, 0xFFFF }, 7, 0 },
    { { 49, 42, 48, 50 }, 0, 0 },
    { { 42, 44, 50, 51 }, 12, 0 },
    { { 44, 45, 51, 52 }, 0, 0 },
    { { 45, 47, 52, 53 }, 13, 0 },
    { { 45, 44, 46, 43 }, 7, 0 },
    { { 43, 42, 49, 0xFFFF }, 7, 0 },
};

s16 D_shelter_b6_nursery_801857A0[24] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    20,
    21,
    22,
    23,
    24,
    28,
    30,
    -1,
};

s16 D_shelter_b6_nursery_801857D0[28] = {
    0,
    1,
    2,
    3,
    4,
    5,
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
    20,
    21,
    29,
    30,
    35,
    37,
    38,
    39,
    41,
    42,
    -1,
};

s16 D_shelter_b6_nursery_80185808[30] = {
    1,
    2,
    3,
    7,
    8,
    11,
    12,
    13,
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
    -1,
};

s16 D_shelter_b6_nursery_80185844[37] = {
    0,
    1,
    2,
    3,
    8,
    11,
    12,
    13,
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
    24,
    25,
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
    -1,
};

s16 * D_shelter_b6_nursery_80185890[4] = {
    D_shelter_b6_nursery_801857A0,
    D_shelter_b6_nursery_801857D0,
    D_shelter_b6_nursery_80185808,
    D_shelter_b6_nursery_80185844,
};

GpGridParams D_shelter_b6_nursery_801858A0 = { NULL, D_shelter_b6_nursery_8018537C, D_shelter_b6_nursery_801853EC, D_shelter_b6_nursery_8018559C, D_shelter_b6_nursery_80185890, -50, 1450, 2, 2, 4000, 43 };

GpViewRec D_shelter_b6_nursery_801858C4[19] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3500, 0x61A8, -2500 } }, 447 },
    { { { { 540, 0, 4060 }, { -792, 4017, 105 }, { -3982, -799, 530 } }, { -6040, 500, 440 } }, 230 },
    { { { { 668, 0, -4041 }, { -249, 4088, -41 }, { 4033, 252, 667 } }, { -940, 1450, 570 } }, 257 },
    { { { { 3940, 0, 1118 }, { 170, 4048, -601 }, { -1105, 624, 3894 } }, { -6140, 1650, 90 } }, 230 },
    { { { { 4042, 0, 659 }, { 298, 3654, -1825 }, { -588, 1850, 3606 } }, { -6090, 2850, -2510 } }, 230 },
    { { { { -364, 0, -4079 }, { -2502, 3234, 223 }, { 3222, 2512, -287 } }, { -5469, 711, 782 } }, 680 },
    { { { { 3615, 0, -1925 }, { 0, 4096, 0 }, { 1925, 0, 3615 } }, { -4740, 870, -450 } }, 329 },
    { { { { -3171, 0, -2592 }, { -979, 3792, 1197 }, { 2400, 1547, -2936 } }, { -5280, 1300, -2310 } }, 329 },
    { { { { 3044, 0, -2740 }, { -2083, 2660, -2314 }, { 1779, 3114, 1977 } }, { -6030, 1220, -1570 } }, 447 },
    { { { { 4020, 0, -781 }, { 149, 4019, 771 }, { 766, -785, 3946 } }, { -6050, 100, 1490 } }, 329 },
    { { { { 1075, 0, 3952 }, { -297, 4084, 80 }, { -3941, -308, 1072 } }, { -7400, 780, 60 } }, 289 },
    { { { { 3641, 0, 1875 }, { -8, 4095, 16 }, { -1875, -18, 3641 } }, { -5890, 1100, 1170 } }, 289 },
    { { { { 4074, 0, 422 }, { -44, 4072, 433 }, { -419, -435, 4051 } }, { -6360, 760, 1430 } }, 257 },
    { { { { 4073, 0, -430 }, { -20, 4091, -198 }, { 429, 199, 4068 } }, { -5560, 1280, 1550 } }, 230 },
    { { { { -182, 0, 4091 }, { 676, 4039, 30 }, { -4035, 677, -179 } }, { -3100, 1350, -80 } }, 257 },
    { { { { 3990, 0, 924 }, { 25, 4094, -109 }, { -923, 112, 3988 } }, { -6560, 1210, 780 } }, 257 },
    { { { { -3964, 0, 1029 }, { 55, 4090, 211 }, { -1027, 219, -3958 } }, { -5890, 1130, -2310 } }, 257 },
    { { { { -3641, 0, 1875 }, { 0, 4096, 0 }, { -1875, 0, -3641 } }, { -5860, 1460, -980 } }, 329 },
    { { { { 2356, 0, 3349 }, { -350, 4073, 246 }, { -3331, -428, 2344 } }, { -6700, 780, 110 } }, 289 },
};

GpSprtCmd D_shelter_b6_nursery_80185B70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80185B80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_80185B90[24] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -120, 760, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -88, 758, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -48, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -16, 775, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -16, 762, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 24, 775, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 24, 757, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 64, 787, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 64, 758, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 96, 759, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -120, 717, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -88, 721, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -48, 720, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -16, 721, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 24, 723, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 64, 724, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 96, 726, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -120, 673, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 693, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 676, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -16, 677, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 24, 679, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 64, 680, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 96, 682, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_80185D70[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_80185D88[44] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 836, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 16, 787, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 16, 806, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 16, 793, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 8, 806, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 16, 777, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 40, 800, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 688, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 656, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 64, 637, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 64, 637, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 547, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 72, 554, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 80, 535, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 80, 510, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 88, 476, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 88, 428, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 96, 434, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 104, 438, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 32, 800, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 32, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 48, 675, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 64, 514, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 80, 493, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 96, 424, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 427, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 96, 415, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 32, 803, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 48, 664, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 64, 491, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 80, 492, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 96, 403, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 32, 785, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 24, 773, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 48, 642, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 849, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -48, 917, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -48, 913, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -32, 915, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -32, 846, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -24, 884, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, -8, 897, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 792, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 24, 799, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_801860F8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 35, 0, 0, { 1, 0 } },
    { 35, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_80186118[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 381, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 326, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 475, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 48, 396, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 64, 399, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 72, 432, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 80, 404, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 374, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 72, 377, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 392, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 400, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 96, 476, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 64, 354, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 80, 360, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 96, 363, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 80, 332, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 96, 346, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_8018626C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80186284[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80186294[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_801862A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_801862B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_801862C4[38] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 32, 129, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 154, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 142, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 24, 135, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 24, 132, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 40, 150, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, 40, 122, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 40, 150, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 151, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 48, 153, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 64, 150, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 8, 64, 150, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, 64, 150, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 64, 150, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 64, 121, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 64, 139, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -24, 88, 150, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, 88, 150, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, 88, 137, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, 88, 122, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 104, 88, 128, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 739, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -8, 745, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -8, 752, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -8, 759, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -8, 766, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 8, 732, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 8, 742, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 8, 752, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 8, 763, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -8, 774, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 32, 722, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 56, 713, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 32, 732, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 32, 742, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 32, 752, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 56, 700, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1163, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_801865BC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 17, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_801865DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_801865EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_801865FC[12] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -16, 511, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -120, 536, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -96, 681, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -72, 861, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -48, 1000, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -24, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -80, 766, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -64, 905, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, -48, 1123, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, -24, 1130, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 0, 977, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 0, 696, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_801866EC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80186704[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_80186714[51] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 664, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 667, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 668, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 670, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -120, 739, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 762, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 785, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 809, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -120, 737, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 758, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 784, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 24, 809, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -120, 731, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -96, 740, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -72, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -48, 762, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -24, 773, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 0, 786, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 24, 798, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 48, 812, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, -16, 787, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 0, 789, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 24, 812, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 48, 804, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -120, 724, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -104, 726, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -120, 722, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -104, 724, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 720, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -104, 723, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -120, 718, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -104, 721, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 810, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 56, 808, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 56, 805, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 718, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -96, 741, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -72, 752, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -48, 763, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -24, 775, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 0, 787, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 800, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 48, 794, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, -120, 717, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, -96, 729, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -72, 737, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -48, 750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -24, 761, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 0, 773, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 24, 784, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 48, 799, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_80186B10[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 51, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_nursery_80186B28[56] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -88, 318, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -72, 319, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -56, 320, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -40, 319, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -24, 320, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 0, 490, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 0, 525, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -8, 320, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -96, 307, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -72, 308, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -48, 309, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -24, 310, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 0, 318, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -120, 269, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -88, 268, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -56, 268, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -24, 271, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 8, 269, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 40, 268, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 72, 268, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 24, 299, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 56, 295, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 16, 360, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 327, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 16, 400, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 40, 358, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 16, 475, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 40, 475, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -120, 307, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -120, 479, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, -120, 436, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, -104, 517, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -120, 454, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, -96, 573, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -120, 462, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -96, 606, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -72, 645, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -48, 658, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -24, 651, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 0, 500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 16, 475, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 32, 475, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -120, 543, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -96, 597, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -72, 725, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -48, 722, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -24, 720, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 0, 550, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 24, 539, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -120, 725, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -96, 729, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -72, 873, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -48, 962, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -24, 957, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 720, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 684, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b6_nursery_80186F88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 56, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80186FA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80186FB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b6_nursery_80186FC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b6_nursery_80186FD0[19] = {
    { { .empty = D_shelter_b6_nursery_80185B70 }, D_shelter_b6_nursery_80185B70, NULL },
    { { .empty = D_shelter_b6_nursery_80185B80 }, D_shelter_b6_nursery_80185B80, NULL },
    { { .elements = D_shelter_b6_nursery_80185B90 }, D_shelter_b6_nursery_80185D70, NULL },
    { { .elements = D_shelter_b6_nursery_80185D88 }, D_shelter_b6_nursery_801860F8, NULL },
    { { .elements = D_shelter_b6_nursery_80186118 }, D_shelter_b6_nursery_8018626C, NULL },
    { { .empty = D_shelter_b6_nursery_80186284 }, D_shelter_b6_nursery_80186284, NULL },
    { { .empty = D_shelter_b6_nursery_80186294 }, D_shelter_b6_nursery_80186294, NULL },
    { { .empty = D_shelter_b6_nursery_801862A4 }, D_shelter_b6_nursery_801862A4, NULL },
    { { .empty = D_shelter_b6_nursery_801862B4 }, D_shelter_b6_nursery_801862B4, NULL },
    { { .elements = D_shelter_b6_nursery_801862C4 }, D_shelter_b6_nursery_801865BC, NULL },
    { { .empty = D_shelter_b6_nursery_801865DC }, D_shelter_b6_nursery_801865DC, NULL },
    { { .empty = D_shelter_b6_nursery_801865EC }, D_shelter_b6_nursery_801865EC, NULL },
    { { .elements = D_shelter_b6_nursery_801865FC }, D_shelter_b6_nursery_801866EC, NULL },
    { { .empty = D_shelter_b6_nursery_80186704 }, D_shelter_b6_nursery_80186704, NULL },
    { { .elements = D_shelter_b6_nursery_80186714 }, D_shelter_b6_nursery_80186B10, NULL },
    { { .elements = D_shelter_b6_nursery_80186B28 }, D_shelter_b6_nursery_80186F88, NULL },
    { { .empty = D_shelter_b6_nursery_80186FA0 }, D_shelter_b6_nursery_80186FA0, NULL },
    { { .empty = D_shelter_b6_nursery_80186FB0 }, D_shelter_b6_nursery_80186FB0, NULL },
    { { .empty = D_shelter_b6_nursery_80186FC0 }, D_shelter_b6_nursery_80186FC0, NULL },
};

GpPointLight D_shelter_b6_nursery_801870B4[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5491, -2000, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3112, 3031, 2949, { 0, 0 } }, 2000, 2750 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5491, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3112, 3031, 2949, { 0, 0 } }, 2000, 2750 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5491, -2000, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3112, 3031, 2949, { 0, 0 } }, 2000, 2750 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3491, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3112, 3031, 2949, { 0, 0 } }, 2000, 2750 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1491, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3112, 3031, 2949, { 0, 0 } }, 2000, 2750 },
};

GpRoomCoordSet D_shelter_b6_nursery_80187294 = { 0, NULL, 5, D_shelter_b6_nursery_801870B4, 0, NULL };

GpObj4C D_shelter_b6_nursery_801872AC[6] = {
    { NULL, NULL, NULL, { 5616, -1504, 4352, 0 }, { { 2032, -1904, 0, 0 }, { -2032, -1904, 0, 0 }, { 2032, 1904, 0, 0 }, { -2032, 1904, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 5488, -1505, 4416, 0 }, { { -2000, -1904, 0, 0 }, { 2000, -1904, 0, 0 }, { -2000, 1904, 0, 0 }, { 2000, 1904, 0, 0 } }, { 0, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 3936, -1441, 96, 0 }, { { 0, -1904, 2144, 0 }, { 0, -1904, -2144, 0 }, { 0, 1904, 2144, 0 }, { 0, 1904, -2144, 0 } }, { -4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 3808, -1601, 128, 0 }, { { 0, -1904, -2256, 0 }, { 0, -1904, 2256, 0 }, { 0, 1904, -2256, 0 }, { 0, 1904, 2256, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2941, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 6193, -1440, 2032, 0 }, { { 2959, -1904, 437, 0 }, { -2961, -1904, -440, 0 }, { 2959, 1904, 437, 0 }, { -2961, 1904, -440, 0 } }, { -601, 0, 4052, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 6017, -1504, 2081, 0 }, { { -2694, -1904, -404, 0 }, { 2686, -1904, 395, 0 }, { -2694, 1904, -404, 0 }, { 2686, 1904, 395, 0 } }, { 602, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 3318, 0, 3, 4, 129, 0 },
};

GpAreaTmdRec D_shelter_b6_nursery_80187474[4] = {
    { 101, 508, 3, 0, { 0, 0 }, D_8014AC88 },
    { 20, 358, 4, 0, { 0, 0 }, D_801539DC },
    { 140, 508, 5, 3, { 0, 0 }, D_8014AC88 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_b6_nursery_801874A4[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C090, D_shelter_b6_nursery_80187474 },
    { NULL, NULL },
    { NULL, NULL },
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

GpObj4C D_shelter_b6_nursery_8018750C[12] = {
    { NULL, NULL, NULL, { 5968, -64, 2416, 0 }, { { -1232, 0, -720, 0 }, { 368, 0, -720, 0 }, { -1232, 0, 688, 0 }, { -240, 0, 1040, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, 4096, 0 }, 1425, 5, 1, 0, 4, 0 },
    { NULL, NULL, NULL, { 6816, -64, 320, 0 }, { { -1024, 0, -864, 0 }, { 288, 0, -864, 0 }, { -1024, 0, 896, 0 }, { 288, 0, 896, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1360, 5, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { 6597, -64, -1143, 0 }, { { -1382, 0, -1023, 0 }, { 1103, 0, 1080, 0 }, { -1359, 0, 584, 0 }, { -730, 0, 1151, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1717, 2, 10, 255, 4, 0 },
    { NULL, NULL, NULL, { 512, -48, 16, 0 }, { { -352, 0, -880, 0 }, { 352, 0, -880, 0 }, { -352, 0, 880, 0 }, { 352, 0, 880, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 947, 0, 25, 18, 2, 0 },
    { NULL, NULL, NULL, { 5392, -64, 1488, 0 }, { { -1872, 0, -528, 0 }, { 1872, 0, -16, 0 }, { -1872, 0, 16, 0 }, { 1872, 0, 528, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1941, 0x8005, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 6287, -64, 4127, 0 }, { { -64, 0, -1188, 0 }, { 1154, 0, 296, 0 }, { -1153, 0, -295, 0 }, { 65, 0, 1189, 0 } }, { 0, 4101, 0, 0 }, { -3513, 0, 2106, 0 }, 1187, 2, 21, 0, 2, 0 },
    { NULL, NULL, NULL, { 4224, -64, -1153, 0 }, { { -1584, 0, -352, 0 }, { 1584, 0, -352, 0 }, { -1584, 0, 352, 0 }, { 1584, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1619, 2, 16, 0, 2, 0 },
    { NULL, NULL, NULL, { 3888, -64, -1632, 0 }, { { -864, 0, 48, 0 }, { 928, 0, 48, 0 }, { -864, 0, 976, 0 }, { 928, 0, 976, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1342, 2, 17, 0, 4, 0 },
    { NULL, NULL, NULL, { 6592, -64, 1728, 0 }, { { -1472, 0, -832, 0 }, { 448, 0, -832, 0 }, { -1472, 0, 256, 0 }, { 448, 0, 256, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1688, 2, 18, 0, 4, 0 },
    { NULL, NULL, NULL, { 6335, -64, 5535, 0 }, { { -991, 0, 349, 0 }, { 493, 0, -869, 0 }, { -492, 0, 870, 0 }, { 992, 0, -348, 0 } }, { 0, 4112, 0, 0 }, { -2598, 0, -3166, 0 }, 1047, 2, 19, 0, 2, 0 },
    { NULL, NULL, NULL, { 4512, -64, 5728, 0 }, { { -960, 0, -1152, 0 }, { 864, 0, -1152, 0 }, { -960, 0, 544, 0 }, { 1792, 0, 544, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1872, 2, 20, 0, 4, 0 },
    { NULL, NULL, NULL, { 5536, -64, 5440, 0 }, { { -1584, 0, -704, 0 }, { 1584, 0, -704, 0 }, { -1584, 0, 704, 0 }, { 1584, 0, 704, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 1731, 2, 14, 0, 131, 0 },
};

GpRoomBoundVec D_shelter_b6_nursery_8018789C[20] = {
    { 19, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 1098, 1116, 1256, 1126 },
    { 1237, 1258, 1320, 1257 },
    { 1601, 1619, 1653, 1616 },
    { 1175, 1200, 1260, 1198 },
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

s32 D_shelter_b6_nursery_8018793C[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

GpRoomParamRec D_shelter_b6_nursery_80187948[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b6_nursery_80187950[1] = {
    { 0, 0, 1, 0, D_shelter_b6_nursery_8018793C },
};

GpRoomParamRec * D_shelter_b6_nursery_80187958[8] = {
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187950,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
};

Task * D_shelter_b6_nursery_80187978 = NULL;

s32 D_shelter_b6_nursery_8018797C = 0;

ShelterB6NurseryStorage7980 D_shelter_b6_nursery_80187980 = { 0 };

GpCoord D_shelter_b6_nursery_801879A0 = { 0 };

ShelterB6NurseryPair D_shelter_b6_nursery_801879F0 = { 0 };

static void func_shelter_b6_nursery_8017E2F4(UiList* list, UiObject* obj);
static void func_shelter_b6_nursery_8017E5F0(UiList* list, UiObject* obj);
static void func_shelter_b6_nursery_8017EE18(u8* str, s32 decimals);
static u8*  func_shelter_b6_nursery_8017EE88(u8* buf, s32 value, s32 decimals);
static void func_shelter_b6_nursery_8017F06C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);

/// Draws row `field_8` of the play-data statistics list: its label, then its
/// value - a time, a count with its suffix, or a percentage with two decimals
/// built from save counters. Row 5 also draws a separator bar under itself and
/// pushes the following rows down. While the list has focus and the row is the
/// selected one, its help line is posted to the holder.
void func_shelter_b6_nursery_8017D72C(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b6_nursery_80184D40,
                D_shelter_b6_nursery_80184D6C,
                D_shelter_b6_nursery_80184D90,
                D_shelter_b6_nursery_80184DC0,
                D_shelter_b6_nursery_80184DF4,
                D_shelter_b6_nursery_80184E28,
                D_shelter_b6_nursery_80184E60,
                D_shelter_b6_nursery_80184E94,
                D_shelter_b6_nursery_80184ECC,
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184CE4);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184D14);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184CEC);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184CF0);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184CF8);
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
            Text_Strcat(p, D_shelter_b6_nursery_80184D3C);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184D04);
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
            Text_Strcat(p, D_shelter_b6_nursery_80184D3C);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184D1C);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184D24);
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
            Text_DrawString(&req, D_shelter_b6_nursery_80184D2C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu.
static const char D_shelter_b6_nursery_8017D610[] = "Play Data";

/// Drawn in place of a percentage for a row holding every recorded use.
static const u8 D_shelter_b6_nursery_8017D61C[] = "100.0%";

/// Draws row `field_8` of an item-usage list: the item's name and icon (unless
/// the panel is in mode 5), its share of all uses as a percentage with two
/// decimals (or the fixed text at 100%), and a gouraud bar whose length is the
/// row's 12-bit bar width. While the row is selected it previews the item and
/// posts its text, and a press of button mask 0x10 plays sound 3 and spawns
/// the `D_8010EFA0` panel for the item.
void func_shelter_b6_nursery_8017DEF8(UiList* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b6_nursery_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_b6_nursery_80184D3C);
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

/// Fills the weapon-usage list's `RoomItemUsage` block from the save's use
/// counters for items 0x80-0x9F. Every item with a name and a non-zero counter
/// is marked seen and appended, the rows are insertion-sorted most-used first,
/// and each row gets its share of all uses in hundredths of a percent and a
/// bar width as a 12-bit fraction of the top row's counter. The counters are
/// halved as needed until the top one is at most 99999, so neither product can
/// overflow.
static void func_shelter_b6_nursery_8017E2F4(UiList* list, UiObject* obj)
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
static void func_shelter_b6_nursery_8017E5F0(UiList* list, UiObject* obj)
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

/// Titles of the usage list: weapons, then Parasite Energy.
static const char D_shelter_b6_nursery_8017D624[] = "Weapon Data";
static const char D_shelter_b6_nursery_8017D630[] = "PE Data";

/// Usage-list panel task: `spawnArg1` 0 lists weapons, otherwise Parasite
/// Energy. On its first tick it allocates the list's work block, spawns the
/// child panel and fills the list; every tick it updates the list, marks the
/// panel for closing on cancel, and tears down any child panel that has
/// finished.
void func_shelter_b6_nursery_8017E910(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &D_shelter_b6_nursery_80184F30;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_shelter_b6_nursery_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, D_shelter_b6_nursery_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            func_shelter_b6_nursery_8017E2F4(list, obj);
        } else {
            func_shelter_b6_nursery_8017E5F0(list, obj);
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
static const char D_shelter_b6_nursery_8017D638[12] = "Telephone\0\x1FQ";

/// Telephone menu task. Until the save has reached demo scene 1 or been
/// cleared once, it only opens the `D_800611E4` panel; after that it lays out
/// and runs the menu list. A finished child selection opens an item prompt
/// (mode 0x11 for entry 0x33, else 0xF) on the first pass; cancel closes the
/// menu with sound 0x3B.
void func_shelter_b6_nursery_8017EAC4(Task* task)
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
    list          = &D_shelter_b6_nursery_80184FB8;
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
        Ui_DrawText(&(obj)->panel, D_shelter_b6_nursery_8017D638);
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

/// Prompt-lines panel task: on its first tick it takes over `Wip_UiHolder` and
/// installs the holder-release exit callback, then draws the prompt lines
/// every tick.
void func_shelter_b6_nursery_8017EDBC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = func_shelter_b6_nursery_8017F4AC;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (or all
/// of them, if fewer) follow it. Does nothing when `decimals` is not positive.
static void func_shelter_b6_nursery_8017EE18(u8* str, s32 decimals)
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

/// Formats the fixed-point `value`, which carries `decimals` fractional
/// digits, into `buf` with a '.' before those digits and the percentage suffix
/// after them. Small values are zero-padded so a digit precedes the point.
/// Returns `buf`.
static u8* func_shelter_b6_nursery_8017EE88(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_b6_nursery_80184D3C);
    return buf;
}

/// Play-data menu task: draws its title, lays out the list and spawns the
/// child panel on its first tick, then updates the list every tick and marks
/// the panel for closing on cancel.
void func_shelter_b6_nursery_8017EF7C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b6_nursery_80184F08;
    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, D_shelter_b6_nursery_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
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

/// Queues a gouraud rectangle one ordering-table slot in front of the panel,
/// at offset (`arg1`, `arg2`) from the panel origin, `arg3` wide and `arg4`
/// high. The left edge takes colour `arg5` and the right edge `arg6`. Nothing
/// is drawn when `arg5` is zero or the width is below 2.
static void func_shelter_b6_nursery_8017F06C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    POLY_G4* prim;
    s16      x;
    s32      y;
    s16      bottom;

    if ((arg5 != 0) && (arg3 >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20.u + arg1 + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22.u;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = arg5;
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = arg5;
        PRIM_COLOR_WORD(prim, 3) = arg6;
        PRIM_COLOR_WORD(prim, 1) = arg6;
        y                       += arg2;
        y++;
        x        = prim->x0 + arg3 - 1;
        prim->y1 = y;
        prim->y0 = y;
        prim->x3 = x;
        prim->x1 = x;
        bottom   = y + arg4 - 1;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14.u + 1, prim);
    }
}

/// Confirm-prompt row: draws its line, and on confirm - once the CD is idle -
/// plays sound 0x16, opens the `D_800611E4` panel and hands state 1 to the
/// owner.
void func_shelter_b6_nursery_8017F170(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CBC, prompt->field_1C, 1, 0);
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

/// Confirm-prompt row: draws its line, and on confirm plays sound 0x16, opens
/// the `D_shelter_b6_nursery_80184F70` panel and puts the owner in state 2.
void func_shelter_b6_nursery_8017F254(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CC4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F70, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Confirm-prompt row: as `func_shelter_b6_nursery_8017F254`, opening the
/// `D_shelter_b6_nursery_80184F8C` panel with argument 0.
void func_shelter_b6_nursery_8017F31C(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CD0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F8C, 0, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Confirm-prompt row: as `func_shelter_b6_nursery_8017F254`, opening the
/// `D_shelter_b6_nursery_80184F8C` panel with argument 1.
void func_shelter_b6_nursery_8017F3E4(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CDC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F8C, 1, 1, 1, obj);
        obj->field_2E        = 6;
        obj->panel.field_0.w = 0;
        obj->owner->state    = 2;
    }
}

/// Exit callback of the prompt-lines panel task: releases `Wip_UiHolder` if the
/// task's panel still holds it, then frees the panel and kills the task.
static void func_shelter_b6_nursery_8017F4AC(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_shelter_b6_nursery_8017F4E8(Task* task)
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
            D_shelter_b6_nursery_80187978 = NULL;
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
            D_shelter_b6_nursery_80187978 =
                Task_SpawnFromTable(D_shelter_b6_nursery_80184FDC, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_shelter_b6_nursery_80187978);
                task->state++;
            } else if (Task_PollKill(D_shelter_b6_nursery_80187978, &poll) != 0) {
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

/// States of the room's message task, run by
/// `func_shelter_b6_nursery_8017FF9C`: install the message table, idle, die.
static const TaskFuncTable3 D_shelter_b6_nursery_8017D6A4 = {
    {
        func_shelter_b6_nursery_8017FEC4,
        func_shelter_b6_nursery_8017FF8C,
        taskKill,
    },
};

s32 func_shelter_b6_nursery_8017FA54(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 flag;

    if (arg2 == 0xA) {
        D_shelter_b6_nursery_8018797C          = 0;
        D_shelter_b6_nursery_80187980.value.field_4  = 0x55160002;
        D_shelter_b6_nursery_80187980.value.field_8  = 0x55160005;
        D_shelter_b6_nursery_80187980.value.field_10 = 0x55160003;
        D_shelter_b6_nursery_80187980.value.field_C  = 0x55160004;
        flag                                   = GameFlag_GetNibble(0xC7);
        if (flag == 1) {
            if (GameFlag_GetNibble(0x83) != 0) {
                Gp_SetBit2Flag(0x22, 1, 4);
            }
            func_800E3FAC(0xA2, 0x31);
            GameFlag_SetNibble(0xC7, 2);
            D_shelter_b6_nursery_80187980.value.field_0 = 6;
            D_shelter_b6_nursery_80187980.value.field_1 = 0xB;
            D_shelter_b6_nursery_80187980.value.field_3 = 0;
            D_shelter_b6_nursery_80187980.value.field_2 = flag;
            Task_SpawnFromTable(D_shelter_b6_nursery_80184FDC, 0, 0x19,
                                &D_shelter_b6_nursery_80187980.value);
            func_80132028();
            func_shelter_b6_nursery_80182D14(0, 0);
            return 0;
        }
        if (GameFlag_GetNibble(0x160) == 0) {
            Gp_SpawnIfCapIdle(0x17, 0);
            GameFlag_SetNibble(0x160, 1);
            return 0;
        }
        D_shelter_b6_nursery_80187980.value.field_0 = 6;
        D_shelter_b6_nursery_80187980.value.field_1 = 0x16;
        D_shelter_b6_nursery_80187980.value.field_3 = 0;
        D_shelter_b6_nursery_80187980.value.field_2 = 0;
        Task_SpawnFromTable(D_shelter_b6_nursery_80184FDC, 0, 0xA,
                            &D_shelter_b6_nursery_80187980.value);
    }
    return 0;
}

void func_shelter_b6_nursery_8017FBC0(Task* arg0)
{
    s32 pan;
    s32 depth;
    s32 viewDepth;

    D_shelter_b6_nursery_801879A0.coord.t[0] = 0x1770;
    D_shelter_b6_nursery_801879A0.coord.t[1] = 0;
    D_shelter_b6_nursery_801879A0.coord.t[2] = -0x33E;
    D_shelter_b6_nursery_801879A0.sub        = &gGfxViewCoord;
    D_shelter_b6_nursery_801879A0.flg        = 0;
    Gp_UpdateCoord(&D_shelter_b6_nursery_801879A0);
    pan   = Gp_GetObjPan(&D_shelter_b6_nursery_801879A0);
    depth = gpGetObjDepth(&D_shelter_b6_nursery_801879A0);
    switch (arg0->state) {
        case 0:
            SndEvt_EnqueueType6(0x55160001, (s8)pan, (s8)depth);
            arg0->state++;
            break;
        case 1:
            if (D_shelter_b6_nursery_8018797C == 0) {
                SndEvt_EnqueueType7(0x55160001, 1);
                taskKill(arg0);
                return;
            }
            if (Mc_SaveData[0].state.at4.loc.view != gGameSession->at4.loc.view) {
                arg0->state++;
            }
            break;
        case 2:
        case 3:
        case 4:
            arg0->state++;
            break;
        case 5:
            viewDepth = D_shelter_b6_nursery_80185034[gGameSession->at4.loc.view];
            if (viewDepth != -1) {
                depth = viewDepth;
            }
            SndEvt_EnqueueTypeA(0x55160001, (s8)pan, (s8)depth);
            arg0->state = 1;
            break;
    }
}

/// Sound task: plays the sound event `spawnArg2` on its first tick and again on
/// tick 0x50, and asks to be killed on tick 0x78.
void func_shelter_b6_nursery_8017FD3C(Task* task)
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

/// Always answers 0.
s32 func_shelter_b6_nursery_8017FDCC(void)
{
    return 0;
}

s32 func_shelter_b6_nursery_8017FDD4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_map_neo_ark_80179B14(src, dst);
    if (src->field_5 == 0) {
        Gp_RunCapCmd1(0xC);
    }
    return 0;
}

s32 func_shelter_b6_nursery_8017FE3C(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    if (msg->field_2 == 1) {
        func_80131E2C();
    }
    if (msg->field_2 == 2) {
        func_80132000();
    }
    if (msg->field_2 == 3 && GameFlag_GetNibble(0xC8) != 0) {
        func_800E8634(&D_8013AF8C, 0, &D_8013BA84);
    }
    return 0;
}

static void func_shelter_b6_nursery_8017FEC4(Task* arg0)
{
    arg0->msgTable = D_shelter_b6_nursery_8018500C;
    Game_SetPtrSlot(arg0, 7);
    Gp_FillAllyHp();
    if (GameFlag_GetNibble(0xC7) == 0) {
        GameFlag_SetNibble(0xC7, 1);
        func_800E8634(&D_80139964, 0, &D_8013A33C);
        GameFlag_SetNibble(3, 0);
        func_800E3FAC(0xA2, 0x30);
    } else if (GameFlag_GetNibble(0xC7) == 1) {
        func_800E8614(&D_8013A84C, 1);
    } else {
        func_800E8614(D_nursery_script_8013A8DC, 1);
    }
    arg0->state++;
}

/// Idle state of the room's message task: does nothing. The unused local
/// reproduces the original's stack frame.
static void func_shelter_b6_nursery_8017FF8C(Task* task)
{
    char pad[0x10];
}

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b6_nursery_8017FF9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_nursery_8017D6A4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

/// Sets the second sprite command's skip-link flag in view 13 for the current
/// room in the first stage table. Only low-byte values 0 and 1 change the flag.
void func_shelter_b6_nursery_80180038(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;
    s32        mode;

    cmd  = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd[1].field_4 = 0;
    } else if (mode == 1) {
        cmd[1].field_4 = 1;
    }
}

void func_shelter_b6_nursery_801800A0(Task* task)
{
    SVECTOR* pos;
    u32      a;
    u32      b;
    s32      angle;
    s32      r;
    s32      i;

    if (task->state == 0) {
        D_80115758                            = 0x601E0;
        D_8011572C                            = 0x601FC;
        D_80115750                            = 0x60218;
        D_shelter_b6_nursery_801879F0.field_0 = 0;
        D_shelter_b6_nursery_801879F0.field_2 = 0;
        task->state                           = 1;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
        case 8:
            if (D_shelter_b6_nursery_801879F0.field_0 != 0) {
                func_shelter_b6_nursery_80180518(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                func_shelter_b6_nursery_80180518(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 6:
        case 10:
            if (D_shelter_b6_nursery_801879F0.field_0 != 0) {
                func_shelter_b6_nursery_8018098C(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                func_shelter_b6_nursery_8018098C(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 12:
            if (task->state == 1) {
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1.value, &D_shelter_b6_nursery_8018504C[1]);
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1.value, &D_shelter_b6_nursery_8018504C[1]);
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1.value, &D_shelter_b6_nursery_8018504C[1]);
                task->state = 2;
            }
            break;
        case 13:
            task->state = 3;
            if (D_shelter_b6_nursery_801879F0.field_2 != 0) {
                for (i = 0; i < 16; i++) {
                    a                                   = Gp_LcgState * 5 + 0x71357911;
                    b                                   = a * 5 + 0x71357911;
                    angle                               = (a >> 16) & 0xFFF;
                    Gp_LcgState                         = b;
                    r                                   = ((b >> 16) & 0xFF) * D_shelter_b6_nursery_801879F0.field_2;
                    D_shelter_b6_nursery_8018504C[6].vx = 0x1C20;
                    D_shelter_b6_nursery_8018504C[6].vy = ((r * rsin(angle)) >> 12) - 0x6D6;
                    D_shelter_b6_nursery_8018504C[6].vz = ((r * rsin(angle)) >> 12) + 0x7D0;
                    Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A5, NULL,
                                (((Gp_LcgState >> 16) & 0x1F) + 8) * D_shelter_b6_nursery_801879F0.field_2,
                                &D_shelter_b6_nursery_8018504C[6]);
                }
                D_shelter_b6_nursery_801879F0.field_0 = 0;
                D_shelter_b6_nursery_801879F0.field_2 = 0;
            }
            break;
        case 15:
            if (!(gDisplayState.animFrame & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[5]);
            }
            break;
        case 17:
            pos = D_shelter_b6_nursery_8018504C;
            func_shelter_b6_nursery_80180518(pos, 0x60, 0x80);
            if (!(gDisplayState.animFrame & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
        case 18:
            if (!(gDisplayState.animFrame & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
    }
    if (task->state == 3 && !(gDisplayState.animFrame & 1)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &(D_shelter_b6_nursery_8018504C + 2)[0]);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &(D_shelter_b6_nursery_8018504C + 2)[1]);
    }
}

/// Draws a glint at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`: two gouraud quads forming a diamond and two gouraud
/// three-point lines across it, of radius `(s16)arg2 * 32` over the depth.
/// The lit vertices pulse in green and blue at a rate of `(s16)arg1` times the
/// animation frame. Nothing is drawn when the projection flags an error.
static void func_shelter_b6_nursery_80180518(SVECTOR* arg0, s32 arg1, s32 arg2)
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
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
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

/// Draws a star-shaped glow at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`: a disc of gouraud wedges at radius `(s16)arg2 * 64`
/// over the depth, a brighter disc at half that radius, and four cross
/// wedges reaching out from an inner radius of `(s16)arg2 * 8` over the depth.
/// The lit vertices pulse in green and blue at a rate of `(s16)arg1` times the
/// animation frame. Nothing is drawn when the projection flags an error.
static void func_shelter_b6_nursery_8018098C(SVECTOR* arg0, s32 arg1, s32 arg2)
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

void func_shelter_b6_nursery_80181314(Task* task)
{
    SVECTOR    step;
    SVECTOR    pos;
    SVECTOR    base;
    TmdObject* obj;
    GpEffWork* work;
    GpCoord*   coord;
    s16        eventState;

    obj   = task->extra.tmd;
    work  = task->spawnArg2.pointer;
    coord = obj->coords;
    if ((Gp_GetViewIndex() & 0xFF) != 0xC) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    {
        eventState = Gp_State1C->eventState;
        if (eventState >= 2) {
            if (eventState >= 4) {
                Gp_ReleaseState1CMem(work, task);
            }
        } else {
            Gp_UpdateCoord(coord);
            if (task->state == 0) {
                obj->flags   &= 0xFF7F;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vx = ((Gp_LcgState >> 16) & 0x3F) + 0x60;
                work->move.vy = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vz = ((Gp_LcgState >> 16) & 0x3F) + 0x20;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->scale   = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
                VectorNormalSS(&work->move, &work->move);
                work->pos.vy = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = -((Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vz = 0x40 - ((Gp_LcgState >> 16) & 0x7F);
                coord->flg   = 0;
                task->state++;
                return;
            }
            Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->pos.vx, 0);
            MatrixNormal(&coord->coord, &coord->coord);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&step);
            coord->coord.t[0] += step.vx;
            coord->coord.t[1] += step.vy;
            coord->coord.t[2] += step.vz;
            coord->flg         = 0;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&step);
            gte_rtv0();
            gte_stsv(&pos);
            base.vx = coord->workm.t[0];
            base.vy = coord->workm.t[1];
            base.vz = coord->workm.t[2];
            pos.vx += base.vx;
            pos.vy += base.vy;
            pos.vz += base.vz;
            if (func_800DE7CC(&pos, &base, &pos, &base) == 1) {
                coord->coord.t[0] -= step.vx;
                coord->coord.t[1] -= step.vy;
                coord->coord.t[2] -= step.vz;
                work->move.vx      = (base.vx >> 1) + (work->move.vx >> 1);
                work->move.vy      = base.vy + (work->move.vy >> 1);
                work->move.vz      = (base.vz >> 1) + (work->move.vz >> 1);
                VectorNormalSS(&work->move, &work->move);
                work->scale = work->scale * 2 / 3;
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&step);
                coord->coord.t[0] += step.vx;
                coord->coord.t[1] += step.vy;
                coord->coord.t[2] += step.vz;
            } else {
                work->move.vy += 0x180;
            }
            if (work->age & 1) {
                if (work->age > 0x40) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A4, coord, ((Gp_LcgState >> 16) & 0x10FF) + 0x02183300, NULL);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A4, coord, ((Gp_LcgState >> 16) & 0x1000) + 0x82101300, NULL);
                }
            }
            work->age++;
        }
    }
}

void func_shelter_b6_nursery_80181820(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        step;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.value & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1.value >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 7:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = ((u32)Gp_LcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            func_shelter_b6_nursery_80181EDC(coord, work->index, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b6_nursery_80182330(coord, work->index, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 8) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

static void func_shelter_b6_nursery_80181EDC(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              vz;
    u16              col;
    u16              row;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)coord->workm.t[1];
    vz                                        = (u16)coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim->tpage = 0x2B;
            prim->clut  = 0x4384;
            prim->code |= 3;
            col         = arg1 % 5;
            row         = arg1 / 5;
            u0          = col * 48;
            v0          = row * 48;
            setUV4(prim, u0, v0 + 0x28, u0 + 0x2F, v0 + 0x28, u0, v0 + 0x57, u0 + 0x2F, v0 + 0x57);
            ang       = arg3;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = block->sx + (u16)block->dx;
            prim->x3  = block->sx - (u16)block->dx;
            prim->y0  = block->sy - (u16)block->dy;
            ang2      = ang + 0x400;
            prim->y3  = block->sy + (u16)block->dy;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = block->sx + (u16)block->dx;
            prim->x2  = block->sx - (u16)block->dx;
            prim->y1  = block->sy - (u16)block->dy;
            prim->y2  = block->sy + (u16)block->dy;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

static void func_shelter_b6_nursery_80182330(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)coord->workm.t[1];
    vz                                        = (u16)coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2B;
            prim->clut  = 0x4385;
            u0          = (arg1 & 7) << 5;
            u1          = u0 + 0x1F;
            setUV4(prim, u0, 0x88, u1, 0x88, u0, 0xA7, u1, 0xA7);
            ang       = arg3;
            block->dx = (((arg2 * 31) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = block->sx + (u16)block->dx;
            prim->x3  = block->sx - (u16)block->dx;
            prim->y0  = block->sy - (u16)block->dy;
            ang2      = ang + 0x400;
            prim->y3  = block->sy + (u16)block->dy;
            block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = block->sx + (u16)block->dx;
            prim->x2  = block->sx - (u16)block->dx;
            prim->y1  = block->sy - (u16)block->dy;
            prim->y2  = block->sy + (u16)block->dy;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b6_nursery_80182730(Task* task)
{
    SVECTOR    step;
    GpEffWork* work;
    GpCoord*   coord;
    s16        eventState;

    work       = task->spawnArg2.pointer;
    eventState = Gp_State1C->eventState;
    coord      = task->extra.tmd->coords;
    if (eventState >= 2) {
        if (eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        if (task->state == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->scale   = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
            work->angle   = task->spawnArg1.value & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = ((Gp_LcgState >> 16) & 0x7F) + 0x40;
            VectorNormalSS(&work->move, &work->move);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->pos.vx = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->pos.vy = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->pos.vz = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            coord->flg   = 0;
            task->state++;
            return;
        }
        Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->pos.vx, 0);
        MatrixNormal(&coord->coord, &coord->coord);
        gte_lddp(work->scale);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&step);
        coord->coord.t[0] += step.vx;
        coord->coord.t[1] += step.vy;
        coord->coord.t[2] += step.vz;
        coord->flg         = 0;
        func_shelter_b6_nursery_801829E4(coord, work->angle, work->period);
        if (coord->coord.t[1] > 0) {
            Gp_ReleaseState1CMem(work, task);
        } else {
            work->move.vy += 0x180;
        }
    }
}

/// Draws one flat grey triangle at `coord`: three corners at 120-degree steps
/// on a circle of radius `scale` in the coordinate's YZ plane, transformed by
/// its world matrix, projected with `GsWSMATRIX` and linked into the ordering
/// table at the triangle's depth with shade `shade`.
static void func_shelter_b6_nursery_801829E4(GpCoord* coord, s16 scale, s16 shade)
{
    _ShelterB6NurseryTriScratch* blk;
    SVECTOR*                     p;
    POLY_F3*                     prim;
    s32                          i;

    SCRATCH_PUSH(_ShelterB6NurseryTriScratch);
    blk = SCRATCH_HEAD(_ShelterB6NurseryTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 3; i++) {
        p     = &blk->v[i];
        p->vx = 0;
        p->vy = rsin(i * 0x555);
        p->vz = rcos(i * 0x555);
        gte_lddp(scale);
        gte_ldsv(p);
        gte_gpf12();
        gte_stsv(p);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(p);
        gte_rtv0();
        gte_stsv(p);
        p->vx = (u16)p->vx + (u16)coord->workm.t[0];
        p->vy = (u16)p->vy + (u16)coord->workm.t[1];
        p->vz = (u16)p->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt();
    prim           = (POLY_F3*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyF3(prim);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_AddTpageShift((P_TAG*)prim, (Gp_LcgState >> 16) & 1, blk->otz);
    }
    SCRATCH_POP(_ShelterB6NurseryTriScratch);
}

void func_shelter_b6_nursery_80182D14(s32 arg0, s32 arg1)
{
    D_shelter_b6_nursery_801879F0.field_0 = arg0;
    D_shelter_b6_nursery_801879F0.field_2 = arg1;
}

/// Flash effect task on the object's coordinate. Over `spawnArg1` ticks it
/// brightens, drawing two fans and a ring in a red-dominant colour that grows
/// each tick; on the last one it flashes the screen. It then fades out as a
/// shrinking billboard, 0x10 a tick, and releases its work block. It also
/// releases it once the room's event state reaches 4, and is frozen while the
/// event state is non-zero.
void func_shelter_b6_nursery_80182D28(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_shelter_b6_nursery_801833F8(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b6_nursery_801833F8(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b6_nursery_80182FCC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_shelter_b6_nursery_801842FC(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring of sixteen gouraud quads around the screen position of the
/// coordinate's world translation, unless the projection flags an error. The
/// vertices at radius `(s16)arg1 * 64` over the depth are black and those at
/// `(s16)(arg1 + arg2) * 64` over the depth take `rgb`.
static void func_shelter_b6_nursery_80182FCC(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Draws a fan of eight gouraud quads around the screen position of the
/// coordinate's world translation, unless the projection flags an error: the
/// centre takes `rgb` and the rim, at radius `arg1 * 64` over the depth,
/// is black.
static void func_shelter_b6_nursery_801833F8(GpCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Trail effect task. On its first tick it allocates two eight-slot histories
/// of view-space coordinates and fills both with the trail's two ends; then
/// every tick it records the current ends in the next slot and draws the band
/// between the histories, until its age reaches `spawnArg1`. It is frozen
/// while the room's event state is 2 or more.
void func_shelter_b6_nursery_8018378C(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_shelter_b6_nursery_801852F4[0].vx;
                objCoord->coord.t[1] = D_shelter_b6_nursery_801852F4[0].vy;
                objCoord->coord.t[2] = D_shelter_b6_nursery_801852F4[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b6_nursery_801852F4[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                {
                    SVECTOR* edge = &D_shelter_b6_nursery_801852F4[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_b6_nursery_80183C7C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the band between two eight-slot coordinate histories as seven gouraud
/// quads, walking back from slot `arg2`; each quad joins two consecutive slots
/// of `arg0` and `arg1`. Brightness falls by 9 per quad from 0x40, and `arg3`
/// scales it per channel: red by `arg3 >> 8`, green by bits 4-5 and blue by
/// bits 0-1. A quad whose projection flags an error is skipped.
static void func_shelter_b6_nursery_80183C7C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Burst effect task on the object's coordinate. On its first tick it spawns
/// effect 0x60076, then either (`spawnArg1` non-zero) a spray of 0x60070
/// sparks in random directions for seven ticks, or two 0x6007C effects and
/// seven ticks of a fading, widening ring. It then releases its work block,
/// as it does once the room's event state reaches 4; it is frozen while the
/// event state is non-zero.
void func_shelter_b6_nursery_80184074(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_shelter_b6_nursery_80182FCC(objCoord, 0x100, 0x100, rgb);
            func_shelter_b6_nursery_80182FCC(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped glow around the screen position of the coordinate's
/// world translation, unless the projection flags an error: a fan of gouraud
/// wedges at radius `arg1 * 64` over the depth in half of `arg2`'s colour, a
/// second at half that radius in the full colour, and four cross wedges from
/// an inner radius of `arg1 * 8` over the depth. Every rim is black.
static void func_shelter_b6_nursery_801842FC(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
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

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
    SCRATCH_POP(RoomBillboardScratch);
}
