#include "rooms/dryfield_factory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "dryfield_factory_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_factory.h"

#include "rooms/room_common.h"

/// The room script task's work block as the prompt-spawning state reads it:
/// `promptKind` is the display mode forwarded to `func_800D4E78`, read signed.
typedef struct RoomUtil21Work {
    /* 0x00 */ byte pad_0[0xE];
    /* 0x0E */ s8   promptKind;
} RoomUtil21Work;

extern TaskDesc D_dryfield_factory_80186E88[];
typedef struct {
    s32  id;
    void (*handler)(Task*);
} FactoryControlMessageEntry;
STATIC_ASSERT_SIZEOF(FactoryControlMessageEntry, 8);

extern FactoryControlMessageEntry D_dryfield_factory_80186EA0[2];
extern OverlayHotspot             D_dryfield_factory_80186EB0[];
extern SVECTOR                    D_dryfield_factory_80186EF8;
extern SVECTOR                    D_dryfield_factory_80186F00;
extern SVECTOR                    D_dryfield_factory_80186F08;

static void func_dryfield_factory_80180A4C(Task* task);
static void func_dryfield_factory_80181538(s32 x, s32 y, s32 variant);
static s32  func_dryfield_factory_80181778(OverlayHotspot* table, s16 x, s16 y);
static void func_dryfield_factory_8018182C(Task* task);
static void func_dryfield_factory_80181938(Task* task);
static void func_dryfield_factory_8018196C(Task* task);
static void func_dryfield_factory_801819BC(Task* task);
static void func_dryfield_factory_80181A24(Task* task);
static void func_dryfield_factory_80181AB8(Task* task);
static void func_dryfield_factory_80181BB4(Task* task);

/// State handlers of the room's script task, run by
/// `func_dryfield_factory_8018169C`: set-up, prompt arming, the idle hotspot
/// scan, prompt spawning, the prompt state, the exit and the wait for the
/// message handler's trigger.
static const TaskFuncTable7 D_dryfield_factory_8017D678 = {
    {
        func_dryfield_factory_8018182C,
        func_dryfield_factory_80181938,
        func_dryfield_factory_80180A4C,
        func_dryfield_factory_8018196C,
        func_dryfield_factory_801819BC,
        func_dryfield_factory_80181A24,
        func_dryfield_factory_80181AB8,
    },
};

void func_dryfield_factory_8018169C(Task*);
void func_dryfield_factory_80181718(Task*);
void func_dryfield_factory_80181768(Task*);

TaskDesc D_dryfield_factory_801826B0 = { 0, 32, func_dryfield_factory_8017D85C, { .model = NULL } };

TaskDesc D_dryfield_factory_801826BC[2] = {
    { 0, 32, func_dryfield_factory_8017DD00, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_factory_801826D4[6] = {
    { 5102, func_dryfield_factory_8017DB08 },
    { 5105, func_dryfield_factory_8017DDA0 },
    { 5104, func_dryfield_factory_8017DDA8 },
    { 5106, func_dryfield_factory_8017DEA8 },
    { 5103, func_dryfield_factory_8017DF14 },
    { 0x7FFFFFFF, NULL },
};

TmdBone D_dryfield_factory_80182704[1] = {
#include "assets/dryfield_factory_model_091F0_skeleton.inc"
};

u32 D_dryfield_factory_80182728[1] = {
#include "assets/dryfield_factory_model_091F0_partVerts.inc"
};

SVECTOR D_dryfield_factory_8018272C[306] = {
#include "assets/dryfield_factory_model_091F0_verts.inc"
};

SVECTOR D_dryfield_factory_801830BC[353] = {
#include "assets/dryfield_factory_model_091F0_normals.inc"
};

u32 D_dryfield_factory_80183BC4[2811] = {
#include "assets/dryfield_factory_model_091F0_stream.inc"
};

TmdSource D_dryfield_factory_801867B0 = {
    0,
    19284,
    0,
    1,
    D_dryfield_factory_80182728,
    D_dryfield_factory_8018272C,
    D_dryfield_factory_801830BC,
    D_dryfield_factory_80182704,
    D_dryfield_factory_80183BC4,
};

TmdBone D_dryfield_factory_801867D4[1] = {
#include "assets/dryfield_factory_model_09610_skeleton.inc"
};

u32 D_dryfield_factory_801867F8[1] = {
#include "assets/dryfield_factory_model_09610_partVerts.inc"
};

SVECTOR D_dryfield_factory_801867FC[25] = {
#include "assets/dryfield_factory_model_09610_verts.inc"
};

SVECTOR D_dryfield_factory_801868C4[28] = {
#include "assets/dryfield_factory_model_09610_normals.inc"
};

u32 D_dryfield_factory_801869A4[139] = {
#include "assets/dryfield_factory_model_09610_stream.inc"
};

TmdSource D_dryfield_factory_80186BD0 = {
    0,
    856,
    0,
    1,
    D_dryfield_factory_801867F8,
    D_dryfield_factory_801867FC,
    D_dryfield_factory_801868C4,
    D_dryfield_factory_801867D4,
    D_dryfield_factory_801869A4,
};

SVECTOR D_dryfield_factory_80186BF4[2] = {
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
};

SVECTOR D_dryfield_factory_80186C04[8] = {
    { 6500, 286, 4350, 0 },
    { 6500, -3308, 4350, 0 },
    { 4464, -3308, 4350, 0 },
    { 4464, 286, 4350, 0 },
    { 4464, 286, 3949, 0 },
    { 4464, -3308, 3949, 0 },
    { 6500, -3308, 3949, 0 },
    { 6500, 286, 3949, 0 },
};

GpGridFace D_dryfield_factory_80186C44[2] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 6, 4, 7 }, 1, 0 },
};

s16 D_dryfield_factory_80186C5C[3] = {
    0,
    1,
    -1,
};

s16* D_dryfield_factory_80186C64[1] = {
    D_dryfield_factory_80186C5C,
};

GpGridParams D_dryfield_factory_80186C68 = { NULL, D_dryfield_factory_80186BF4, D_dryfield_factory_80186C04, D_dryfield_factory_80186C44, D_dryfield_factory_80186C64, -4464, -3949, 1, 1, 4000, 2 };

SVECTOR D_dryfield_factory_80186C8C[4] = {
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_dryfield_factory_80186CAC[8] = {
    { -750, 0, 1950, 0 },
    { -750, -1100, 1950, 0 },
    { -750, -1100, -2191, 0 },
    { -750, 0, -2191, 0 },
    { 750, -1100, -2191, 0 },
    { 750, 0, -2191, 0 },
    { 750, -1100, 1950, 0 },
    { 750, 0, 1950, 0 },
};

GpGridFace D_dryfield_factory_80186CEC[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_dryfield_factory_80186D1C[5] = {
    0,
    1,
    2,
    3,
    -1,
};

s16 D_dryfield_factory_80186D28[4] = {
    0,
    2,
    3,
    -1,
};

s16* D_dryfield_factory_80186D30[2] = {
    D_dryfield_factory_80186D1C,
    D_dryfield_factory_80186D28,
};

GpGridParams D_dryfield_factory_80186D38 = { NULL, D_dryfield_factory_80186C8C, D_dryfield_factory_80186CAC, D_dryfield_factory_80186CEC, D_dryfield_factory_80186D30, 750, 2191, 1, 2, 4000, 4 };

SVECTOR D_dryfield_factory_80186D5C[4] = {
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_dryfield_factory_80186D7C[8] = {
    { -750, 0, 3050, 0 },
    { -750, -1100, 3050, 0 },
    { -750, -1100, -1950, 0 },
    { -750, 0, -1950, 0 },
    { 750, -1100, -1950, 0 },
    { 750, 0, -1950, 0 },
    { 750, -1100, 3050, 0 },
    { 750, 0, 3050, 0 },
};

GpGridFace D_dryfield_factory_80186DBC[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_dryfield_factory_80186DEC[4] = {
    0,
    1,
    2,
    -1,
};

s16 D_dryfield_factory_80186DF4[4] = {
    0,
    2,
    3,
    -1,
};

s16* D_dryfield_factory_80186DFC[2] = {
    D_dryfield_factory_80186DEC,
    D_dryfield_factory_80186DF4,
};

GpGridParams D_dryfield_factory_80186E04 = { NULL, D_dryfield_factory_80186D5C, D_dryfield_factory_80186D7C, D_dryfield_factory_80186DBC, D_dryfield_factory_80186DFC, 750, 1950, 1, 2, 4000, 4 };

TaskDesc D_dryfield_factory_80186E28[8] = {
    { 0, 192, func_dryfield_factory_8017FC18, { .model = NULL } },
    { 0, 192, func_dryfield_factory_801807DC, { .model = NULL } },
    { 0, 192, func_dryfield_factory_80180920, { .model = NULL } },
    { 0, 192, func_dryfield_factory_8017FDDC, { .model = NULL } },
    { 1, 192, func_dryfield_factory_8018072C, { .model = &D_dryfield_factory_801867B0 } },
    { 2, 192, func_dryfield_factory_8018001C, { .model = NULL } },
    { 0, 192, func_dryfield_factory_80180964, { .model = NULL } },
    { 1, 192, func_dryfield_factory_80180784, { .model = &D_dryfield_factory_80186BD0 } },
};

TaskDesc D_dryfield_factory_80186E88[1] = {
    { 0, 192, func_dryfield_factory_80181718, { .model = NULL } },
};

TaskDesc D_dryfield_factory_80186E94[1] = {
    { 0, 192, func_dryfield_factory_8018169C, { .model = NULL } },
};

FactoryControlMessageEntry D_dryfield_factory_80186EA0[2] = {
    { 5107, func_dryfield_factory_80181768 },
    { 0x7FFFFFFF, NULL },
};

OverlayHotspot D_dryfield_factory_80186EB0[6] = {
    { -68, -63, 16, 16, 0, 1, 0 },
    { -27, -63, 16, 16, 1, 1, 0 },
    { 13, -63, 16, 16, 2, 1, 0 },
    { 46, -80, 34, 32, 3, 0, 0 },
    { -38, 0, 72, 48, 4, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

SVECTOR D_dryfield_factory_80186EF8 = { 395, -1630, 846, 0 };

SVECTOR D_dryfield_factory_80186F00 = { 5910, -1308, 5649, 0 };

SVECTOR D_dryfield_factory_80186F08 = { 5910, -1404, 5649, 0 };

GpRoomObjRec D_dryfield_factory_80186F10[2] = {
    { &D_dryfield_factory_80187BF8, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
    { &D_dryfield_factory_80187BF8, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
};

u8 D_dryfield_factory_80186F30[20] = {
    1,
    13,
    14,
    15,
    5,
    16,
    17,
    8,
    9,
    10,
    11,
    12,
    2,
    3,
    4,
    6,
    7,
    18,
    19,
    0,
};

u8* D_dryfield_factory_80186F44[2] = {
    D_8010CAF8,
    D_dryfield_factory_80186F30,
};

GpRoomCoordRec D_dryfield_factory_80186F4C[2] = {
    { D_dryfield_factory_8018A28C, D_dryfield_factory_8018A2A4 },
    { D_dryfield_factory_8018A28C, D_dryfield_factory_8018A2A4 },
};

GpViewCountRec D_dryfield_factory_80186F5C[2] = {
    { { .bytes = { 19, 0 } } },
    { { .bytes = { 19, 0 } } },
};

GpWarpRec D_dryfield_factory_80186F60[3] = {
    { { .words = { 3072, 5178, 0, 1454 } }, { 0, 0, 0, 0 }, { .words = { 1024, 4530, 0, 1200 } }, { 0, 0, 0, 0 }, 0x52170002, 0x52170001, 0, 2, 0, 474 },
    { { .words = { 1024, 642, 0, 7493 } }, { 0, 0, 0, 0 }, { .words = { 1024, 4530, 0, 1200 } }, { 0, 0, 0, 0 }, 0x52170004, 0x52170003, 0x52170005, 7, 0, 475 },
    { { .words = { 3072, 5445, 0, 6866 } }, { 0, 0, 0, 0 }, { .words = { 2048, 5420, 0, 6000 } }, { 0, 0, 0, 0 }, 0x52170014, 0x52170006, 0, 6, 2, 473 },
};

SVECTOR D_dryfield_factory_80187008[28] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, -1936, -3610, 0 },
    { 0, 0, -4096, 0 },
    { 0, -4096, 0, 0 },
    { 2048, 0, -3547, 0 },
    { 4096, 0, 0, 0 },
    { 546, 0, -4059, 0 },
    { 4094, 0, -140, 0 },
    { -3577, 0, 1996, 0 },
    { -1138, 0, 3935, 0 },
    { -4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { -3193, 0, 2565, 0 },
    { 0, -1936, 3610, 0 },
    { -1, 0, -4096, 0 },
    { 4096, 0, 0, 0 },
    { -3974, 0, 993, 0 },
    { -835, 0, 4010, 0 },
    { 0, 0, 4096, 0 },
    { -4094, 0, 126, 0 },
    { -3563, 0, 2021, 0 },
    { 1832, 0, 3664, 0 },
    { 1177, 0, -3923, 0 },
};

SVECTOR D_dryfield_factory_801870E8[170] = {
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
    { 2400, -1207, 6252, 0 },
    { 2400, -1828, 6585, 0 },
    { 4750, -1828, 6585, 0 },
    { 4750, -1207, 6252, 0 },
    { 700, -2100, 3950, 0 },
    { 700, -3500, 3950, 0 },
    { 1700, -3500, 3950, 0 },
    { 1700, -2100, 3950, 0 },
    { 4750, -1207, 5609, 0 },
    { 2400, -1207, 5610, 0 },
    { 6444, 0, 8733, 0 },
    { 4950, 0, 8733, 0 },
    { 4950, 0, 0x2F99, 0 },
    { 6444, 0, 0x2F99, 0 },
    { 4950, 0, 4741, 0 },
    { 6444, 0, 4741, 0 },
    { 6444, 0, -222, 0 },
    { 4950, 0, -222, 0 },
    { 2267, 0, 4741, 0 },
    { 2267, 0, -222, 0 },
    { 2267, 0, 8733, 0 },
    { 2267, 0, 0x2F99, 0 },
    { -444, 0, 4741, 0 },
    { -444, 0, -222, 0 },
    { -444, 0, 8733, 0 },
    { -444, 0, 0x2F99, 0 },
    { 808, 200, 9858, 0 },
    { 808, -2391, 9858, 0 },
    { 2672, -2391, 0x2AB7, 0 },
    { 2672, 200, 0x2AB7, 0 },
    { 4750, 200, 6728, 0 },
    { 4750, -1207, 7473, 0 },
    { 4750, -1207, 7950, 0 },
    { 4750, 200, 7950, 0 },
    { -169, 200, 8006, 0 },
    { -169, -1200, 8006, 0 },
    { 1000, -1200, 8163, 0 },
    { 1000, 200, 8163, 0 },
    { 1148, -1200, 0x30CC, 0 },
    { 1148, 200, 0x30CC, 0 },
    { 3980, 200, 9150, 0 },
    { 3980, -1456, 9150, 0 },
    { 3059, -1456, 7500, 0 },
    { 3059, 200, 7500, 0 },
    { 6111, -1456, 7500, 0 },
    { 6111, 200, 7500, 0 },
    { 400, 200, -56, 0 },
    { 400, -1838, -56, 0 },
    { 400, -1838, 2007, 0 },
    { 400, 200, 2007, 0 },
    { 6162, 200, 9780, 0 },
    { 6162, -1456, 9780, 0 },
    { 2400, -1207, 7473, 0 },
    { 2400, -1828, 7139, 0 },
    { 4750, -1828, 7139, 0 },
    { 600, 200, 2717, 0 },
    { 600, -1200, 2717, 0 },
    { 600, -1200, 4259, 0 },
    { 600, 200, 4259, 0 },
    { 4500, -2100, 3950, 0 },
    { 4500, 200, 3950, 0 },
    { 1700, 200, 3950, 0 },
    { 1505, -1200, 0x2E87, 0 },
    { 6867, -1200, 0x2E87, 0 },
    { 6668, -1200, 0x2A65, 0 },
    { 1747, -1200, 0x2A65, 0 },
    { 1747, 200, 0x2A65, 0 },
    { 6668, 200, 0x2A65, 0 },
    { 4500, 200, 4750, 0 },
    { 4500, -1361, 4750, 0 },
    { 1800, -1361, 4750, 0 },
    { 1800, 200, 4750, 0 },
    { 4500, -2100, 4350, 0 },
    { 4500, 200, 4350, 0 },
    { 2400, 200, 6976, 0 },
    { 2400, 200, 5610, 0 },
    { 6040, 200, 0x2B81, 0 },
    { 6040, -1920, 0x2B81, 0 },
    { 5825, -1920, 0x2A75, 0 },
    { 5825, 200, 0x2A75, 0 },
    { 5825, -1920, 8988, 0 },
    { 5825, 200, 8988, 0 },
    { 3997, -1888, 1982, 0 },
    { 6132, -1888, 1982, 0 },
    { 6132, -1, 1982, 0 },
    { 3997, -1, 1982, 0 },
    { 2400, 200, 7950, 0 },
    { 2400, -1207, 7950, 0 },
    { 4500, -3500, 3950, 0 },
    { 4500, -3500, 4350, 0 },
    { 5799, -4500, 4250, 0 },
    { 5799, -4500, 0, 0 },
    { 5799, 0, 0, 0 },
    { 5799, 0, 4250, 0 },
    { 6000, -4500, 4250, 0 },
    { 6000, 0, 4250, 0 },
    { 6000, 0, 4100, 0 },
    { 6000, 0, 0x2EE0, 0 },
    { 6000, -4500, 0x2EE0, 0 },
    { 6000, -4500, 4100, 0 },
    { 4750, 200, 5609, 0 },
    { 4500, -1363, 4350, 0 },
    { 0, -4500, 0x2E40, 0 },
    { 6000, -4500, 0x2E40, 0 },
    { 6000, 0, 0x2E40, 0 },
    { 0, 0, 0x2E40, 0 },
    { 700, 200, 3950, 0 },
    { 700, -2100, 4350, 0 },
    { 700, 200, 4350, 0 },
    { 171, -4500, 0, 0 },
    { 171, -4500, 0x2EE0, 0 },
    { 171, 0, 0x2EE0, 0 },
    { 171, 0, 0, 0 },
    { 1700, -1363, 4350, 0 },
    { 1700, 200, 4350, 0 },
    { 6140, -1, 3391, 0 },
    { 6140, -1888, 3391, 0 },
    { 4027, -1888, 2951, 0 },
    { 4027, -1, 2951, 0 },
    { 6000, -4500, 172, 0 },
    { 0, -4500, 172, 0 },
    { 0, 0, 172, 0 },
    { 6000, 0, 172, 0 },
    { 6342, 200, 682, 0 },
    { 6342, -2334, 682, 0 },
    { 2770, -2334, 682, 0 },
    { 2770, 200, 682, 0 },
    { 2270, -2334, -200, 0 },
    { 2270, 200, -200, 0 },
    { 1700, -2100, 4350, 0 },
    { 1700, -3500, 4350, 0 },
    { 700, -3500, 4350, 0 },
    { -50, -2100, 3950, 0 },
    { -50, -3500, 3950, 0 },
    { -50, 200, 3950, 0 },
    { -50, -3500, 4350, 0 },
    { -50, -2100, 4350, 0 },
    { -50, 200, 4350, 0 },
    { 900, 0, 2700, 0 },
    { 900, -1900, 2700, 0 },
    { -100, -1900, 3200, 0 },
    { -100, 0, 3200, 0 },
    { -100, 0, 1700, 0 },
    { -100, -1900, 1700, 0 },
    { 900, -1900, 2000, 0 },
    { 900, 0, 2000, 0 },
};

GpGridFace D_dryfield_factory_80187638[72] = {
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 25, 26, 24, 27 }, 6, 0 },
    { { 29, 30, 28, 31 }, 7, 0 },
    { { 32, 33, 27, 24 }, 8, 0 },
    { { 35, 36, 34, 37 }, 8, 2 },
    { { 39, 40, 38, 41 }, 8, 2 },
    { { 38, 35, 39, 34 }, 8, 2 },
    { { 38, 41, 42, 43 }, 8, 2 },
    { { 42, 44, 38, 35 }, 8, 3 },
    { { 44, 45, 35, 36 }, 8, 2 },
    { { 42, 43, 46, 47 }, 8, 2 },
    { { 46, 48, 42, 44 }, 8, 2 },
    { { 48, 49, 44, 45 }, 8, 2 },
    { { 51, 52, 50, 53 }, 9, 0 },
    { { 55, 56, 54, 57 }, 10, 0 },
    { { 59, 60, 58, 61 }, 11, 0 },
    { { 60, 62, 61, 63 }, 12, 0 },
    { { 65, 66, 64, 67 }, 13, 0 },
    { { 66, 68, 67, 69 }, 7, 0 },
    { { 71, 72, 70, 73 }, 10, 0 },
    { { 75, 65, 74, 64 }, 14, 0 },
    { { 76, 77, 24, 25 }, 15, 0 },
    { { 27, 26, 55, 78 }, 10, 0 },
    { { 80, 81, 79, 82 }, 10, 0 },
    { { 84, 85, 83, 31 }, 7, 0 },
    { { 87, 88, 86, 89 }, 8, 0 },
    { { 89, 88, 90, 91 }, 7, 0 },
    { { 93, 94, 92, 95 }, 16, 0 },
    { { 97, 84, 96, 83 }, 10, 0 },
    { { 24, 33, 98, 99 }, 15, 0 },
    { { 101, 102, 100, 103 }, 17, 0 },
    { { 102, 104, 103, 105 }, 15, 0 },
    { { 107, 108, 106, 109 }, 7, 0 },
    { { 98, 76, 24, 0xFFFF }, 15, 0 },
    { { 98, 110, 76, 111 }, 15, 0 },
    { { 55, 54, 27, 0xFFFF }, 10, 0 },
    { { 112, 113, 83, 96 }, 10, 0 },
    { { 115, 116, 114, 117 }, 15, 0 },
    { { 78, 77, 55, 76 }, 18, 0 },
    { { 114, 117, 118, 119 }, 16, 0 },
    { { 121, 122, 120, 123 }, 15, 0 },
    { { 54, 124, 27, 32 }, 10, 0 },
    { { 125, 93, 97, 92 }, 10, 0 },
    { { 127, 128, 126, 129 }, 7, 0 },
    { { 28, 131, 130, 132 }, 10, 0 },
    { { 33, 32, 99, 124 }, 19, 0 },
    { { 134, 135, 133, 136 }, 20, 0 },
    { { 94, 137, 95, 138 }, 21, 0 },
    { { 140, 141, 139, 142 }, 22, 0 },
    { { 144, 145, 143, 146 }, 23, 0 },
    { { 141, 106, 142, 109 }, 24, 0 },
    { { 111, 56, 76, 55 }, 8, 0 },
    { { 148, 149, 147, 150 }, 16, 0 },
    { { 152, 150, 151, 149 }, 25, 0 },
    { { 56, 111, 57, 110 }, 16, 0 },
    { { 154, 155, 153, 131 }, 16, 0 },
    { { 157, 29, 156, 28 }, 7, 0 },
    { { 130, 158, 28, 156 }, 7, 0 },
    { { 155, 159, 131, 160 }, 16, 0 },
    { { 161, 132, 160, 131 }, 16, 0 },
    { { 85, 138, 31, 153 }, 15, 0 },
    { { 138, 97, 153, 96 }, 16, 0 },
    { { 113, 154, 96, 153 }, 16, 0 },
    { { 30, 112, 31, 83 }, 7, 0 },
    { { 163, 164, 162, 165 }, 26, 0 },
    { { 167, 168, 166, 169 }, 27, 0 },
    { { 168, 163, 169, 162 }, 10, 0 },
};

s16 D_dryfield_factory_80187998[44] = {
    0,
    1,
    2,
    3,
    4,
    5,
    7,
    8,
    10,
    12,
    13,
    15,
    16,
    24,
    28,
    29,
    32,
    33,
    34,
    37,
    41,
    47,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    57,
    58,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    -1,
};

s16 D_dryfield_factory_801879F0[58] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    19,
    20,
    21,
    22,
    23,
    24,
    26,
    27,
    28,
    29,
    32,
    33,
    34,
    38,
    39,
    40,
    41,
    43,
    46,
    47,
    49,
    50,
    51,
    52,
    53,
    55,
    56,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    -1,
};

s16 D_dryfield_factory_80187A64[35] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    8,
    9,
    11,
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
    25,
    26,
    27,
    30,
    31,
    34,
    38,
    39,
    40,
    43,
    48,
    51,
    56,
    59,
    -1,
};

s16 D_dryfield_factory_80187AAC[16] = {
    0,
    1,
    2,
    3,
    4,
    5,
    9,
    14,
    17,
    18,
    21,
    30,
    31,
    48,
    51,
    -1,
};

s16 D_dryfield_factory_80187ACC[32] = {
    0,
    1,
    2,
    3,
    4,
    5,
    8,
    10,
    11,
    12,
    13,
    15,
    29,
    32,
    33,
    37,
    41,
    42,
    44,
    45,
    46,
    47,
    50,
    53,
    54,
    55,
    57,
    58,
    66,
    67,
    68,
    -1,
};

s16 D_dryfield_factory_80187B0C[48] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    19,
    22,
    23,
    25,
    26,
    27,
    29,
    32,
    33,
    34,
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
    50,
    52,
    53,
    55,
    56,
    59,
    66,
    67,
    68,
    -1,
};

static void func_dryfield_factory_80180BA4(RoomRect* rect, u8 r, u8 g, u8 b);
static void func_dryfield_factory_80180DE8(Task* task, s16 step);
static void func_dryfield_factory_801810D8(Task* task);
static void func_dryfield_factory_80181C14(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Idle state of the room's script task. It counts down the delay the prompt
/// state armed and, once that is spent and no cap is playing, hit-tests the
/// room's hotspots under the cursor: a confirmed hit copies the hotspot's `id`
/// and `promptKind` into the work block and advances to state 3, and the
/// cancel button leaves for state 5.
static void func_dryfield_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = D_80114D28;
    OverlayHotspot*         hs     = D_dryfield_factory_80186EB0;
    NightFactoryScriptWork* st     = (NightFactoryScriptWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_dryfield_factory_80181778(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_C      = hs->id;
                    st->field_E      = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
static void func_dryfield_factory_80180BA4(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

static void func_dryfield_factory_80180DE8(Task* task, s16 step)
{
    s32 id;
    s32 state;

    if (GameFlag_GetNibble(0x48) != 0) {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (!(GameFlag_GetNibble(0x49) & 2)) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) | 2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData[0].state.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData[0].state.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(8, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (GameFlag_GetNibble(0x49) & 2) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) & ~2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData[0].state.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData[0].state.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(9, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) ^ 1);
                if (GameFlag_GetNibble(0x47) == 0) {
                    Mc_SaveData[0].state.at4.loc.view = 0x12;
                } else {
                    Mc_SaveData[0].state.at4.loc.view = 0x13;
                }
                state       = 6;
                task->state = state;
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 1);
                state       = 2;
                task->state = state;
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                state       = 2;
                task->state = state;
                break;
        }
    } else {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(8, 0, 0);
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(9, 0, 0);
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(0xA, 0, 0);
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 0);
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                break;
        }
        task->state = 2;
    }
}

static void func_dryfield_factory_801810D8(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1.value) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28[port];
        pad    = &Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                                          = 0;
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos = prompt->screen.packed;
                    *statep                                                             = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_dryfield_factory_80181538(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad -- the room's on-screen action prompt icon --
/// at (`x`, `y`) into the head of the current OT. `variant` selects the palette,
/// 0x3C87 when it is 2 and 0x3C88 otherwise, and 0 draws nothing at all.
static void func_dryfield_factory_80181538(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Sets the skip-link byte on the second sprite command of view 9 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.loc.stage` is 2.
void func_dryfield_factory_80181620(s32 arg0)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

/// Runs the room script task's current state. The seven handlers are copied
/// onto the stack first, so the call goes through a local table rather than
/// through `.rodata`.
void func_dryfield_factory_8018169C(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_factory_8017D678;
    sp.funcs[task->state](task);
}

void func_dryfield_factory_80181718(Task* task)
{
    TaskFunc states[2] = { func_dryfield_factory_80181BB4, func_dryfield_factory_801810D8 };

    states[task->state](task);
}

/// Message 0x13F3 handler of the room's script task: raises the one-shot
/// trigger `field_A` in its work block, which the script's wait state consumes.
void func_dryfield_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}

static s32 func_dryfield_factory_80181778(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// State 0 of the room's script task: allocates its work block, spawns the
/// child task, publishes the script's message table, picks the global mode
/// byte from game flag 0x48, advances, and clears the hotspot hits while
/// holding the HUD for the cutscene.
static void func_dryfield_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    OverlayHotspot*         hs;

    work = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = Task_SpawnFromTable(D_dryfield_factory_80186E88, 0, 1, 0);
    task->work              = (TaskIdMap*)work;
    task->msgTable          = D_dryfield_factory_80186EA0;
    if (GameFlag_GetNibble(0x48) == 0) {
        Mc_SaveData[0].state.at4.loc.view = 0xC;
    } else {
        Mc_SaveData[0].state.at4.loc.view = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_factory_80186EB0; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->field_8              = 0;
}

/// Arms the action prompt for a hotspot and steps the caller's script on one
/// state: marks the prompt as highlighted (`mode` 1) for the fixed target id
/// 0x80 and resets the on-screen position, which `func_800D4E78` fills in again
/// when the prompt is actually spawned.
static void func_dryfield_factory_80181938(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
static void func_dryfield_factory_8018196C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    RoomUtil21Work*   work   = (RoomUtil21Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Prompt state of the room's script task: clears the cursor highlight and,
/// while `func_800D4EC0` still reports a prompt on screen, runs the cap step
/// the work block names; otherwise it returns to the idle state 2. Either way
/// it re-arms the idle state's delay.
static void func_dryfield_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28[0].mode     = 0;
    D_80114D28[0].targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}

static void func_dryfield_factory_80181A24(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgAlly3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill((Task*)arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

/// Wait state of the room's script task: once the one-shot trigger `field_A`
/// has been raised by the script's message handler, picks the global mode
/// byte from game flag 0x48, re-arms the idle delay, consumes the trigger and
/// returns the script to its idle state 2.
static void func_dryfield_factory_80181AB8(Task* task)
{
    RoomActionPrompt*       prompt;
    NightFactoryScriptWork* work;

    prompt           = D_80114D28;
    work             = (NightFactoryScriptWork*)task->work;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            Mc_SaveData[0].state.at4.loc.view = 0xC;
        } else {
            Mc_SaveData[0].state.at4.loc.view = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}

/// Sets the skip-link byte on the second sprite command of view 11 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.loc.stage` is 2.
void func_dryfield_factory_80181B38(s32 arg0)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

/// Resets both action-prompt slots before a script's first cursor scan and steps
/// the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1).
static void func_dryfield_factory_80181BB4(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0                     = 0;
        prompt->field_4                     = 0;
        prompt->targetId                    = 0x100;
        prompt->field_E                     = 0xF;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = 1;
    }
    task->state = task->state + 1;
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges, tinted. `arg1` is a
/// signed half-extent; on-screen radii are `(s16)arg1 * 64 / otz` (outer) and
/// `(s16)arg1 * 8 / otz` (inner). `arg2` packs the tint into four nibbles,
/// `[shift][r][g][b]`: each colour nibble is scaled to 8 bits by `<< 4`, and
/// bit 0 of `gDisplayState.animFrame` is added to all three channels shifted
/// left by the top nibble, so the disc flickers on alternating frames. The
/// outer ring draws at half brightness first and full brightness second; the
/// inner cross uses the halved colour throughout.
static void func_dryfield_factory_80181C14(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                packed;
    s32                blend;
    s32                size;
    s32                otz;
    s32                rOuter;
    s32                rInner;
    s32                ang;
    s32                t;
    s32                t2;
    s32                r;
    s32                g;
    s32                b;
    s32                rh;
    s32                gh;
    s32                bh;

    {
        void** scratch;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        block   = (RoomDraw05Scratch*)(*scratch = head - 0x14);
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        size          = (s16)arg1;
        otz           = block->otz + 1;
        rOuter        = (size * 64) / otz;
        block->otz    = otz;
        ds            = &gDisplayState;
        blend         = ds->animFrame;
        block->rOuter = rOuter;
        rInner        = (size * 8) / block->otz;
        packed        = arg2 << 16;
        blend         = blend & 1;
        blend         = blend << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = rInner;
        ang           = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            rh = (u8)r >> 1;
            gh = (u8)g >> 1;
            bh = (u8)b >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rh, gh, bh);
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
            setRGB2(prim, r, g, b);
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

        r   = (u8)rh;
        g   = (u8)gh;
        b   = (u8)bh;
        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang - 0x400)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ang - 0x400)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x400)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x400)) >> 13);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang + 0x400)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang + 0x400)) >> 11);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x800)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x800)) >> 12);
            ang     += 0x800;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Per-frame effect: draws up to three glowing discs at fixed points in the
/// room. The draw set is selected by the stage-visit byte
/// `gGameSession->at4.loc.view` taken as a bit index, and each group also gates on a
/// story flag, so a disc only appears on the visits and after the event that
/// the flag records.
void func_dryfield_factory_801825F0(Task* task)
{
    s32 state;

    state = 1 << gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x48) != 0 && (state & 0x15068) != 0) {
        func_dryfield_factory_80181C14(&D_dryfield_factory_80186EF8, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(0x4A) == 1) {
            func_dryfield_factory_80181C14(&D_dryfield_factory_80186F00, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(0x4A) == 2) {
            func_dryfield_factory_80181C14(&D_dryfield_factory_80186F08, 0x80, 0x50A0);
        }
    }
}
