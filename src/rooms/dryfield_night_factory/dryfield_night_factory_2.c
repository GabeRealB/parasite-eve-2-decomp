#include "rooms/dryfield_night_factory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "dryfield_night_factory_private.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

#include "rooms/dryfield_factory.h"

#include "rooms/room_common.h"
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

/// The single-entry `TaskDesc` table the room's script task spawns its child
/// task from: the prompt state machine `func_dryfield_night_factory_80181718`.
extern TaskDesc D_dryfield_night_factory_80186E94[];

/// The room's 0xFFFF-terminated hotspot table.
extern OverlayHotspot D_dryfield_night_factory_80186EBC[];

typedef struct {
    s32  id;
    void (*handler)(Task*);
} FactoryControlMessageEntry;
STATIC_ASSERT_SIZEOF(FactoryControlMessageEntry, 8);

extern FactoryControlMessageEntry D_dryfield_night_factory_80186EAC[2];

extern TaskDesc gRoomEventTaskDesc;

extern TaskDesc   D_dryfield_night_factory_80186E4C[];
extern GpMsgEntry D_dryfield_night_factory_80186E64[];

/// The world-space points the room's three glow discs are drawn at.
extern SVECTOR D_dryfield_night_factory_80186F04;
extern SVECTOR D_dryfield_night_factory_80186F0C;
extern SVECTOR D_dryfield_night_factory_80186F14;

static void func_dryfield_night_factory_80180438(Task* arg0);
static void func_dryfield_night_factory_801809EC(Task* task);
static void func_dryfield_night_factory_80180A4C(Task* task);
static void func_dryfield_night_factory_80181938(Task* task);
static void func_dryfield_night_factory_8018196C(Task* task);
static void func_dryfield_night_factory_801819BC(Task* task);
static void func_dryfield_night_factory_80181A24(Task* task);
static void func_dryfield_night_factory_80181AB8(Task* task);

static void func_dryfield_night_factory_8018182C(Task* task);

s32  func_dryfield_night_factory_80180574(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_night_factory_8018080C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_night_factory_80180814(Task*, s32, s32, TaskMessageArg);
s32  func_dryfield_night_factory_80180914(Task*, s32, s32, s32);
s32  func_dryfield_night_factory_80180980(Task*, s32, DirectionActionRequest* request, TaskMessageArg);
void func_dryfield_night_factory_8018076C(Task*);
void func_dryfield_night_factory_8018169C(Task*);
void func_dryfield_night_factory_80181718(Task*);
void func_dryfield_night_factory_80181768(Task*);

extern GpGridFace D_dryfield_night_factory_80187630[72];
extern SVECTOR    D_dryfield_night_factory_80187000[28];
extern SVECTOR    D_dryfield_night_factory_801870E0[170];
extern s16*       D_dryfield_night_factory_80187BD0[8];

TmdBone D_dryfield_night_factory_801826BC[1] = {
#include "assets/dryfield_night_factory_model_091A8_skeleton.inc"
};

u32 D_dryfield_night_factory_801826E0[1] = {
#include "assets/dryfield_night_factory_model_091A8_partVerts.inc"
};

SVECTOR D_dryfield_night_factory_801826E4[306] = {
#include "assets/dryfield_night_factory_model_091A8_verts.inc"
};

SVECTOR D_dryfield_night_factory_80183074[353] = {
#include "assets/dryfield_night_factory_model_091A8_normals.inc"
};

u32 D_dryfield_night_factory_80183B7C[2811] = {
#include "assets/dryfield_night_factory_model_091A8_stream.inc"
};

TmdSource D_dryfield_night_factory_80186768 = {
    0,
    19284,
    0,
    1,
    D_dryfield_night_factory_801826E0,
    D_dryfield_night_factory_801826E4,
    D_dryfield_night_factory_80183074,
    D_dryfield_night_factory_801826BC,
    D_dryfield_night_factory_80183B7C,
};

TmdBone D_dryfield_night_factory_8018678C[1] = {
#include "assets/dryfield_night_factory_model_095C8_skeleton.inc"
};

u32 D_dryfield_night_factory_801867B0[1] = {
#include "assets/dryfield_night_factory_model_095C8_partVerts.inc"
};

SVECTOR D_dryfield_night_factory_801867B4[25] = {
#include "assets/dryfield_night_factory_model_095C8_verts.inc"
};

SVECTOR D_dryfield_night_factory_8018687C[28] = {
#include "assets/dryfield_night_factory_model_095C8_normals.inc"
};

u32 D_dryfield_night_factory_8018695C[139] = {
#include "assets/dryfield_night_factory_model_095C8_stream.inc"
};

TmdSource D_dryfield_night_factory_80186B88 = {
    0,
    856,
    0,
    1,
    D_dryfield_night_factory_801867B0,
    D_dryfield_night_factory_801867B4,
    D_dryfield_night_factory_8018687C,
    D_dryfield_night_factory_8018678C,
    D_dryfield_night_factory_8018695C,
};

SVECTOR D_dryfield_night_factory_80186BAC[2] = {
#include "assets/dryfield_night_factory_collision_09660_normals.inc"
};

SVECTOR D_dryfield_night_factory_80186BBC[8] = {
#include "assets/dryfield_night_factory_collision_09660_verts.inc"
};

GpGridFace D_dryfield_night_factory_80186BFC[2] = {
#include "assets/dryfield_night_factory_collision_09660_faces.inc"
};

s16 D_dryfield_night_factory_80186C14[4] = {
#include "assets/dryfield_night_factory_collision_09660_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_factory_80186C14[i])
s16* D_dryfield_night_factory_80186C1C[1] = {
#include "assets/dryfield_night_factory_collision_09660_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_factory_80186C20 = { NULL, D_dryfield_night_factory_80186BAC, D_dryfield_night_factory_80186BBC, D_dryfield_night_factory_80186BFC, D_dryfield_night_factory_80186C1C, -4464, -3949, 1, 1, 4000, 2 };

SVECTOR D_dryfield_night_factory_80186C44[4] = {
#include "assets/dryfield_night_factory_collision_09730_normals.inc"
};

SVECTOR D_dryfield_night_factory_80186C64[8] = {
#include "assets/dryfield_night_factory_collision_09730_verts.inc"
};

GpGridFace D_dryfield_night_factory_80186CA4[4] = {
#include "assets/dryfield_night_factory_collision_09730_faces.inc"
};

s16 D_dryfield_night_factory_80186CD4[10] = {
#include "assets/dryfield_night_factory_collision_09730_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_factory_80186CD4[i])
s16* D_dryfield_night_factory_80186CE8[2] = {
#include "assets/dryfield_night_factory_collision_09730_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_factory_80186CF0 = { NULL, D_dryfield_night_factory_80186C44, D_dryfield_night_factory_80186C64, D_dryfield_night_factory_80186CA4, D_dryfield_night_factory_80186CE8, 750, 2191, 1, 2, 4000, 4 };

SVECTOR D_dryfield_night_factory_80186D14[4] = {
#include "assets/dryfield_night_factory_collision_097FC_normals.inc"
};

SVECTOR D_dryfield_night_factory_80186D34[8] = {
#include "assets/dryfield_night_factory_collision_097FC_verts.inc"
};

GpGridFace D_dryfield_night_factory_80186D74[4] = {
#include "assets/dryfield_night_factory_collision_097FC_faces.inc"
};

s16 D_dryfield_night_factory_80186DA4[8] = {
#include "assets/dryfield_night_factory_collision_097FC_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_factory_80186DA4[i])
s16* D_dryfield_night_factory_80186DB4[2] = {
#include "assets/dryfield_night_factory_collision_097FC_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_factory_80186DBC = { NULL, D_dryfield_night_factory_80186D14, D_dryfield_night_factory_80186D34, D_dryfield_night_factory_80186D74, D_dryfield_night_factory_80186DB4, 750, 1950, 1, 2, 4000, 4 };

TaskDesc D_dryfield_night_factory_80186DE0[8] = {
    { 0, 192, func_dryfield_night_factory_8017F330, { .model = NULL } },
    { 0, 192, func_dryfield_night_factory_8017FEF4, { .model = NULL } },
    { 0, 192, func_dryfield_night_factory_80180038, { .model = NULL } },
    { 0, 192, func_dryfield_night_factory_8017F4F4, { .model = NULL } },
    { TASK_BODY_TMD, 192, func_dryfield_night_factory_8017FE44, { .model = &D_dryfield_night_factory_80186768 } },
    { TASK_BODY_COORD, 192, func_dryfield_night_factory_8017F734, { .model = NULL } },
    { 0, 192, func_dryfield_night_factory_8018007C, { .model = NULL } },
    { TASK_BODY_TMD, 192, func_dryfield_night_factory_8017FE9C, { .model = &D_dryfield_night_factory_80186B88 } },
};

TaskDesc gRoomEventTaskDesc = { 0, 32, roomEventTask, { .model = NULL } };

TaskDesc D_dryfield_night_factory_80186E4C[2] = {
    { 0, 32, func_dryfield_night_factory_8018076C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_night_factory_80186E64[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_factory_80180574 },
    { 5105, func_dryfield_night_factory_8018080C },
    { 5104, func_dryfield_night_factory_80180814 },
    { 5106, func_dryfield_night_factory_80180914 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_factory_80180980 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_night_factory_80186E94[1] = {
    { 0, 192, func_dryfield_night_factory_80181718, { .model = NULL } },
};

TaskDesc D_dryfield_night_factory_80186EA0[1] = {
    { 0, 192, func_dryfield_night_factory_8018169C, { .model = NULL } },
};

FactoryControlMessageEntry D_dryfield_night_factory_80186EAC[2] = {
    { 5107, func_dryfield_night_factory_80181768 },
    { 0x7FFFFFFF, NULL },
};

OverlayHotspot D_dryfield_night_factory_80186EBC[6] = {
    { -68, -63, 16, 16, 0, 1, 0 },
    { -27, -63, 16, 16, 1, 1, 0 },
    { 13, -63, 16, 16, 2, 1, 0 },
    { 46, -80, 34, 32, 3, 0, 0 },
    { -38, 0, 72, 48, 4, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

SVECTOR D_dryfield_night_factory_80186F04 = { 395, -1630, 846, 0 };

SVECTOR D_dryfield_night_factory_80186F0C = { 5910, -1308, 5649, 0 };

SVECTOR D_dryfield_night_factory_80186F14 = { 5910, -1404, 5649, 0 };

u8* D_dryfield_night_factory_80186F1C[2] = {
    D_8010CAF8,
    D_8010CAF8,
};

GpRoomCoordRec D_dryfield_night_factory_80186F24[2] = {
    { D_dryfield_night_factory_80189C88, D_dryfield_night_factory_8018A0C8 },
    { D_dryfield_night_factory_80189C88, D_dryfield_night_factory_8018A0C8 },
};

GpRoomObjRec D_dryfield_night_factory_80186F34[2] = {
    { &D_dryfield_night_factory_80187BF0, D_dryfield_night_factory_80189CA0, D_dryfield_night_factory_8018A168, NULL },
    { &D_dryfield_night_factory_80187BF0, D_dryfield_night_factory_80189CA0, D_dryfield_night_factory_8018A168, NULL },
};

GpViewCountRec D_dryfield_night_factory_80186F54[2] = {
    { { .bytes = { 19, 0 } } },
    { { .bytes = { 19, 0 } } },
};

GpWarpRec D_dryfield_night_factory_80186F58[3] = {
    { { .words = { 3072, 5178, 0, 1454 } }, { 0, 0, 0, 0 }, { .words = { 768, 3952, 0, 1200 } }, { 0, 0, 0, 0 }, 0x53170002, 0x53170001, 0, 2, 0, 474 },
    { { .words = { 1024, 642, 0, 7493 } }, { 0, 0, 0, 0 }, { .words = { 1024, 1100, 0, 7060 } }, { 0, 0, 0, 0 }, 0x53170004, 0x53170003, 0x53170005, 7, 0, 475 },
    { { .words = { 3072, 5445, 1, 6866 } }, { 0, 0, 0, 0 }, { .words = { 1024, 1100, 0, 7060 } }, { 0, 0, 0, 0 }, 0x53170014, 0x53170006, 0, 6, 2, 473 },
};

SVECTOR D_dryfield_night_factory_80187000[28] = {
#include "assets/dryfield_night_factory_collision_0A630_normals.inc"
};

SVECTOR D_dryfield_night_factory_801870E0[170] = {
#include "assets/dryfield_night_factory_collision_0A630_verts.inc"
};

GpGridFace D_dryfield_night_factory_80187630[72] = {
#include "assets/dryfield_night_factory_collision_0A630_faces.inc"
};

s16 D_dryfield_night_factory_80187990[288] = {
#include "assets/dryfield_night_factory_collision_0A630_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_factory_80187990[i])
s16* D_dryfield_night_factory_80187BD0[8] = {
#include "assets/dryfield_night_factory_collision_0A630_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_factory_80187BF0 = { NULL, D_dryfield_night_factory_80187000, D_dryfield_night_factory_801870E0, D_dryfield_night_factory_80187630, D_dryfield_night_factory_80187BD0, 444, 222, 2, 4, 4000, 72 };

GpViewRec D_dryfield_night_factory_80187C14[19] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3057, 0x44F4, -6028 } }, 240 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -1035, 0, 3962 }, { 929, 3981, 242 }, { -3852, 960, -1006 } }, { -5890, 2089, -0x284C } }, 240 },
    { { { { -489, 0, -4066 }, { -641, 4044, 77 }, { 4015, 646, -483 } }, { -337, 1837, -9758 } }, 240 },
    { { { { 619, 0, -4048 }, { -727, 4029, -111 }, { 3982, 736, 608 } }, { -4642, 1552, -5446 } }, 257 },
    { { { { 1741, 0, -3707 }, { 404, 4071, 190 }, { 3685, -446, 1731 } }, { -4747, 1512, -9880 } }, 282 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
};

SpriteBatch D_dryfield_night_factory_80187EC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80187ED0[13] = {
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 104, 40, 300, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 40, 700, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, -48, -32, 0x3847, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, 16, 1058, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 16, 1021, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, 32, 0x3318, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -8, 16, 0x2749, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -24, 8, 1150, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, 8, 1140, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -160, -120, 1037, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, -136, -120, 1125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 200 } }, -112, -120, 1162, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -96, -120, 1212, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80187FD4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 5, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 2, 0 } },
    { 9, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188004[12] = {
    { 143, 0x3FC0, { .fields = { 72, 240 } }, -160, -120, 50, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 88 } }, 72, 32, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 200 } }, 120, -120, 2000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 200 } }, 64, -120, 2000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 96, -120, 2000, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 56, 1150, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 40, 1157, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 24, 1155, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 8, 1153, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -8, 1147, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -24, 1151, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -40, 1150, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_801880F4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 1, 0, 0, { 2, 0 } },
    { 2, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_8018811C[82] = {
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -72, 1487, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -128, -24, 1455, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -96, -40, 1455, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 80, -64, 820, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -40, 844, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 908, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 1284, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -16, 1237, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, -40, 1230, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, 16, 1083, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 987, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -64, 884, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -16, -120, 792, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188784[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 3, 0 } },
    { 27, 40, 0, 0, { 2, 0 } },
    { 67, 11, 0, 0, { 4, 0 } },
    { 78, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_801887BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_801887CC[32] = {
    { 142, 0x3FC0, { .fields = { 96, 32 } }, -160, 88, 425, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -64, 96, 425, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -48, 1327, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -56, 1351, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -88, 1216, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, -120, 1083, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -24, 1383, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 32 } }, -64, 88, 625, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 96, 56 } }, -160, 64, 500, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 24 } }, -64, 64, 0, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188A4C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 4, 0 } },
    { 3, 7, 0, 0, { 1, 0 } },
    { 10, 15, 0, 0, { 3, 0 } },
    { 25, 4, 0, 0, { 2, 0 } },
    { 29, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188A84[39] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 96, 500, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -48, 2377, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, -32, 2425, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, -32, 2450, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, -16, 2375, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -16, 2375, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -16, 2475, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 56, -32, 2250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 8, 1935, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 0, 1935, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 0, 1935, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -8, 1935, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -64, -88, 1833, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -32, -88, 1791, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, -88, 1791, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 8, -88, 1740, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -64, -24, 1982, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -32, -24, 1911, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1911, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 8, -24, 1848, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -88, 1710, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 56, -88, 1675, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -56, 1675, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -56, 1675, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -16, 1780, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -16, 1780, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -88, 1638, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -40, 1719, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -8, 1792, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 40, 1200, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 56, 1200, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 72, 1200, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 88, 1200, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, 104, 1200, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 48, 1200, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 64, 1200, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 80, 1200, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 96, 1200, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -128, 64, 1200, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188D90[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 7, 0, 0, { 0, 0 } },
    { 8, 4, 0, 0, { 5, 0 } },
    { 12, 14, 0, 0, { 1, 0 } },
    { 26, 3, 0, 0, { 4, 0 } },
    { 29, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188DD0[12] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -128, -120, 125, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, -72, 125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, -64, 125, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -24, 125, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, -8, 125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, 0, 125, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 24, 125, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, 56, 125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 88, 125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -144, 64, 125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 125, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -136, 96, 125, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188EC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188ED8[4] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, 16, 1062, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 16, 1062, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 16, 1062, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 32, 16, 1062, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80188F28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80188F80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80188F90[78] = {
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -96, -40, 1450, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -128, -24, 1450, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, -72, 1475, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 80, -64, 884, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 0, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, -40, 1250, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, -16, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 1250, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -40, 1250, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_801895A8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 11, 0, 0, { 0, 0 } },
    { 27, 40, 0, 0, { 2, 0 } },
    { 67, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_801895D8[32] = {
    { 141, 0x3FC0, { .fields = { 72, 24 } }, -72, 96, 757, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 32 } }, -160, 88, 667, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 112 } }, -160, -24, 0, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 120 } }, -72, -24, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 0, -24, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 104 } }, 8, 16, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 24 } }, -16, -88, 0, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -64, 0, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, 48, -88, 0, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -48, 1311, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_80189858[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 10, 0, 0, { 2, 0 } },
    { 17, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_factory_80189880[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_factory_80189890[17] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1300, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 48, 1750, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -56, 64, 1750, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 8, 1325, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 1325, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 16, 1312, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 16, 1320, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 1322, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1315, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1300, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 32, 1362, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 40, 1375, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1312, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 40, 1275, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1282, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1287, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 216 } }, -112, -120, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_factory_801899E4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 10, 0, 0, { 0, 0 } },
    { 13, 3, 0, 0, { 2, 0 } },
    { 16, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

static void func_dryfield_night_factory_80180DE8(Task* task, s16 step);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// State handlers of the room entry task: set-up, an empty tick and
/// `taskKill`.
static const TaskFuncTable3 D_dryfield_night_factory_8017D638 = {
    { func_dryfield_night_factory_80180438, func_dryfield_night_factory_801809EC, taskKill },
};

/// Room entry task: publishes the room's message table, claims game pointer
/// slot 7 and parks a fresh one-word slot at `Task::work` (also kept in
/// `D_dryfield_night_factory_8018A7E8`) for the poller to fill. It then picks the spawn tables for
/// the session variant (`stage == 2` or not), spawns entries 4 and 5 of the
/// first, and passes progress nibble 0x48 to the variant's view-sprite helper.
static void func_dryfield_night_factory_80180438(Task* arg0)
{
    Task** slot;

    arg0->msgTable = D_dryfield_night_factory_80186E64;
    Game_SetPtrSlot(arg0, 7);
    slot       = (D_dryfield_night_factory_8018A7E8 = memCalloc(4, 0));
    arg0->work = slot;
    if (gGameSession->location.loc.stage == 2) {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_factory_80186E28;
    } else {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186DE0;
    }
    if (gGameSession->location.loc.stage == 2) {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186E94;
    } else {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186EA0;
    }
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 4, 0, D_dryfield_night_factory_8018A7E8);
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 5, 0, 0);
    if (gGameSession->location.loc.stage == 2) {
        func_dryfield_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
    } else {
        func_dryfield_night_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
    }
    arg0->state++;
}

/// Filters a warp request: copies `in` to `out`, choosing the destination room
/// for area 0x19 from the stage variant and progress flags, and for area 0x18
/// from game flag 0x7A. Area 0x18 is refused with cap slot 4 until game flag
/// 0x4A reaches 2, area 0x16 with cap command 0xD while game flag 0x37 is
/// clear, and area 0x19 goes through the event gate with the room's own
/// request. Any other warp answers 1.
s32 func_dryfield_night_factory_80180574(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u8           variant;

    *out = *in;
    if (in->areaId == 0x19) {
        variant = gGameSession->location.loc.stage;
        if (variant == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->room = variant;
                } else {
                    out->room = 1;
                }
            }
        } else if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->areaId == 0x18) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (GameFlag_GetNibble(0x7A) < 4) {
                out->room = 1;
            } else {
                out->room = 2;
            }
        }
        if (in->areaId == 0x18) {
            if (GameFlag_GetNibble(0x4A) != 2) {
                if (in->queryOnly != ROOM_EVENT_EXECUTE) {
                    return 0;
                }
                Gp_StartCapSlot(4, 1, 0);
                Gp_SetNibbleIf(in->flagId, 2);
                return 0;
            }
        }
    }
    if (in->areaId == 0x16) {
        if (GameFlag_GetNibble(0x37) == 0) {
            if (in->queryOnly != ROOM_EVENT_EXECUTE) {
                return 0;
            }
            Gp_SetNibbleIf(in->flagId, 2);
            Gp_RunCapCmd1(0xD);
            return 0;
        }
    }
    if (in->areaId == 0x19) {
        req.field_0 = 0xE;
        req.field_4 = 0xE;
        req.field_8 = 0x52170013;
        req.field_C = 0x52170003;
        req.flagId  = -0x30;
        req.itemId  = 0;
        return roomEventGate(&req, in);
    }
    return 1;
}

/// Spawns the script task from the table the room entry task selected, parks it
/// in the entry task's slot, and kills itself once that task has gone.
void func_dryfield_night_factory_8018076C(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_night_factory_8018A7E8 = Task_SpawnFromTable(D_dryfield_night_factory_8018A7E0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_night_factory_8018A7E8, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

s32 func_dryfield_night_factory_8018080C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Command handler for the night factory room, reached from the room's command
/// table (`D_dryfield_night_factory_80186E64`, id 0x13F0) with the command in
/// `$a2`.
///
/// Cases 1/2/3/5/12 spawn an actor out of whichever spawn table the session
/// selected (`D_..._A7E4`, written by `func_dryfield_night_factory_80180438`)
/// at index 2/3/1/0/6, handing the command on as `Task_SpawnFromTable`'s third
/// argument. Case 6 silences both characters' weapons and spawns the factory's
/// own table `D_..._80186E4C` at index 0 instead -- that table's task is the
/// `func_dryfield_night_factory_8018076C` poller. Case 12 only acts while
/// progress flag 0x49 is 1, and silences the player's and the ally's weapon
/// before spawning. Every other command does nothing.
///
/// The `goto`s are the target's shape: every path shares the single `return 0`
/// at `end`, so the exit block is the only place `$v0` is zeroed.
s32 func_dryfield_night_factory_80180814(Task* arg0, s32 arg1, s32 cmd, TaskMessageArg arg3)
{
    TaskDesc* table;
    s32       idx;

    switch (cmd) {
        case 1:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 2;
            break;
        case 2:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 3;
            break;
        case 3:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 1;
            break;
        case 5:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 0;
            break;
        case 6:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Gp_MsgAllyWeapon(0);
            Gp_MsgAlly3F3(0);
            Task_SpawnFromTable(D_dryfield_night_factory_80186E4C, 0, 0, 0);
            goto end;
        case 12:
            if (GameFlag_GetNibble(0x49) == 1) {
                Gp_MsgPlayerWeapon(0);
                Gp_MsgAllyWeapon(0);
                table = D_dryfield_night_factory_8018A7E4;
                idx   = 6;
                break;
            }
            goto end;
        default:
            goto end;
    }
    Task_SpawnFromTable(table, idx, cmd, 0);
end:
    return 0;
}

/// Message handler: command 7 plays sound 0x52170007, and command 21 plays
/// 0x52170015 and sets game flag 0x4A to 2.
s32 func_dryfield_night_factory_80180914(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 7:
            Gp_EnqueueStageSnd6(0x52170007, 0, 0);
            break;
        case 21:
            Gp_EnqueueStageSnd6(0x52170015, 0, 0);
            GameFlag_SetNibble(0x4A, 2);
            break;
    }
    return 0;
}

/// Message handler: the first message with `actionId` 1 while game flag 0x2C is
/// clear starts cap 0xB, sets the flag and plays sound 0x5217000A.
s32 func_dryfield_night_factory_80180980(Task* task, s32 msgId, DirectionActionRequest* request, TaskMessageArg arg3)
{
    if ((request->actionId == 1) && (GameFlag_GetNibble(0x2C) == 0)) {
        Gp_SpawnIfCapIdle(0xB, 1);
        GameFlag_SetNibble(0x2C, 1);
        func_800E3FAC(0xA2, 0xA);
        SndEvt_EnqueueType6(0x5217000A, 0, 0);
    }
    return 0;
}

/// The room entry task's per-frame state, which does nothing.
static void func_dryfield_night_factory_801809EC(Task* task)
{
}

/// Runs the room entry task's current state, through a copy of its handler
/// table on the stack.
void func_dryfield_night_factory_801809F4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D638;
    sp.funcs[task->state](task);
}

/// Idle state of the room's script, state 2 of
/// `D_dryfield_night_factory_8017D678`. It holds the prompt idle for the
/// `field_8` frames the prompt states armed -- decrementing that countdown
/// first and bailing out while it is still non-zero or while a cap is playing
/// -- and otherwise hit-tests the room's hotspot table.
///
/// A confirmed hit (`buttons[0].state == 2`) copies the hotspot's `id` and
/// `promptKind` into the work block and advances to state 3; with nothing under
/// the cursor the prompt merely highlights (`mode` 1). `buttons[1].state == 2`
/// leaves the scan by advancing to state 5.
static void func_dryfield_night_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = D_80114D28;
    OverlayHotspot*         hs     = D_dryfield_night_factory_80186EBC;
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
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
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

/// State handlers of the room's script task, run by
/// `func_dryfield_night_factory_8018169C`: set-up, prompt arming, the idle
/// hotspot scan, prompt spawning, the prompt state, the exit and the wait for
/// the message handler's trigger.
static const TaskFuncTable7 D_dryfield_night_factory_8017D678 = {
    {
        func_dryfield_night_factory_8018182C,
        func_dryfield_night_factory_80181938,
        func_dryfield_night_factory_80180A4C,
        func_dryfield_night_factory_8018196C,
        func_dryfield_night_factory_801819BC,
        func_dryfield_night_factory_80181A24,
        func_dryfield_night_factory_80181AB8,
    },
};

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Runs cap step `step` of the room's script, picking the sound, the progress
/// flags and the cap slot for the step.
static void func_dryfield_night_factory_80180DE8(Task* task, s16 step)
{
    s32 id;
    s32 state;

    if (GameFlag_GetNibble(0x48) != 0) {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
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
                if (gGameSession->location.loc.stage == 2) {
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
                if (gGameSession->location.loc.stage == 2) {
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
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(8, 0, 0);
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(9, 0, 0);
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
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

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

void func_dryfield_night_factory_80181620(s32 show)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(show & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}

/// Runs the script task's current state. The seven handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_dryfield_night_factory_8018169C(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_night_factory_8017D678;
    sp.funcs[task->state](task);
}

/// The prompt task the script spawns: resets both action-prompt slots, then
/// moves and draws the cursors every frame.
void func_dryfield_night_factory_80181718(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

    states[task->state](task);
}

/// Script message handler: raises the one-shot trigger the cursor state
/// consumes.
void func_dryfield_night_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// Task callback of the descriptor at `D_dryfield_night_factory_80186E94`:
/// allocates the script work block, spawns the room's child task, picks the
/// global mode byte from game flag 0x48, steps the task on one state and clears
/// the room's hotspot list.
static void func_dryfield_night_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    OverlayHotspot*         hs;

    work = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = Task_SpawnFromTable(D_dryfield_night_factory_80186E94, 0, 1, 0);
    task->work              = work;
    task->msgTable          = D_dryfield_night_factory_80186EAC;
    if (GameFlag_GetNibble(0x48) == 0) {
        Mc_SaveData[0].state.at4.loc.view = 0xC;
    } else {
        Mc_SaveData[0].state.at4.loc.view = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_night_factory_80186EBC; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->field_8              = 0;
}

/// Script state: highlights the action prompt (`mode` 1, target id 0x80),
/// clears its screen position and steps the script on one state.
static void func_dryfield_night_factory_80181938(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Script state: drops the prompt's highlight and spawns the action prompt at
/// the cursor position with the display mode of the confirmed hotspot, then
/// moves the script to state 4.
static void func_dryfield_night_factory_8018196C(Task* task)
{
    RoomActionPrompt*       prompt = D_80114D28;
    NightFactoryScriptWork* work   = (NightFactoryScriptWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}

/// Runs the prompt state of the night factory script: drops the highlight the
/// previous state left in `D_80114D28` and, while `func_800D4EC0` still reports
/// a prompt on screen, hands the task to the cap step `field_C` names. Once the
/// prompt is gone the task advances to state 2 instead, and either way the work
/// block's `field_8` is set to 0xA.
static void func_dryfield_night_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28[0].mode     = 0;
    D_80114D28[0].targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_night_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}

/// Script state that ends the scene: gives the player back their weapon and
/// the HUD, releases the display, kills the prompt task and asks for this one
/// to be killed.
static void func_dryfield_night_factory_80181A24(Task* arg0)
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

/// Script state that waits for the one-shot trigger: keeps the prompt hidden
/// and, once `field_A` is raised, consumes it, re-arms the countdown and goes
/// back to the idle state.
static void func_dryfield_night_factory_80181AB8(Task* task)
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

void func_dryfield_night_factory_80181B38(s32 show)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(show & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}

#include "../../shared/action_prompt_reset.inc.c"

#include "../../shared/glow_draw_tinted_disc.inc.c"

/// Per-frame effect: refreshes the task's composed matrix, then draws
/// one of three glowing discs at fixed points in the room. The draw set is
/// selected by the stage-visit byte `gGameSession->location.loc.view` taken as a bit
/// index, and each of the three groups also gates on a story flag, so a disc
/// only appears on the visits and after the event that the flag records.
void func_dryfield_night_factory_801825F0(Task* task)
{
    s32 state;

    state = 1 << gGameSession->location.loc.view;
    Gp_UpdateCoord(task->extra.coordBody->coord);
    if (GameFlag_GetNibble(0x48) != 0 && (state & 0x15068) != 0) {
        glowDrawTintedDisc(&D_dryfield_night_factory_80186F04, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(0x4A) == 1) {
            glowDrawTintedDisc(&D_dryfield_night_factory_80186F0C, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(0x4A) == 2) {
            glowDrawTintedDisc(&D_dryfield_night_factory_80186F14, 0x80, 0x50A0);
        }
    }
}
