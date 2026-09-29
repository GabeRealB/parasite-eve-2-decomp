#include "rooms/dryfield_warehouse.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_warehouse_private.h"

#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// Work block of the warehouse's cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_warehouse_8017E090` and parked in `Task::work`. `owner` is
/// the slot-3 game pointer the task dispatches its messages to; `field_4` is
/// the script step `func_dryfield_warehouse_8017DBB0` runs and `field_6` its
/// sub-step, both set by `func_dryfield_warehouse_8017E3F4`; `field_8` and
/// `field_E` are per-step frame counters; `playerEffActive` records that the
/// script has hidden the player's effects and they are to be restored.
typedef struct DwhWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ u16   field_4;
    /* 0x06 */ u16   field_6;
    /* 0x08 */ u16   field_8;
    /* 0x0A */ byte  pad_A[0x2];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ u16   field_E;
} DwhWork;
STATIC_ASSERT_SIZEOF(DwhWork, 0x10);

extern GpAnimSet* D_dryfield_warehouse_8017F848[2];
extern GpXformArg D_dryfield_warehouse_8017F850;
extern GpXformArg D_dryfield_warehouse_8017F868;

extern GpEvsCmd D_dryfield_warehouse_8017F880[];
extern GpEvsCmd D_dryfield_warehouse_8017FA00[];

/// Points in the space of the coordinate drawn under: ring centres, one per
/// circle, for the ring drawer, and prism corners for the prism drawer, which
/// the room's only caller points at `[8..15]`.
extern SVECTOR D_dryfield_warehouse_8017FB2C[];
/// Ring radii, parallel to the centres.
extern s16 D_dryfield_warehouse_8017FBAC[];

extern GpGridParams D_dryfield_warehouse_801802A8[1];
extern GpGridParams D_dryfield_warehouse_801809AC[1];
extern GpGridParams D_dryfield_warehouse_80181038[1];

void func_dryfield_warehouse_8017E090(Task*);
void func_dryfield_warehouse_8017E22C(Task*);
void func_dryfield_warehouse_8017E308(Task*);

void func_dryfield_warehouse_8017DA58(s32);
void func_dryfield_warehouse_8017E3F4(s16);

GpMsgEntry D_dryfield_warehouse_8017F554[3] = {
    { 5102, func_dryfield_warehouse_8017D824 },
    { 5105, func_dryfield_warehouse_8017D764 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_warehouse_8017F56C[2] = {
    { 0, 32, func_dryfield_warehouse_8017D8D4, { .model = NULL } },
    { 0, 32, func_dryfield_warehouse_8017D5E8, { .model = NULL } },
};

AnimationPackedPose D_dryfield_warehouse_8017F584[2] = {
#include "assets/dryfield_warehouse_animation_02260_bank1.inc"
};

AnimationPackedRotation D_dryfield_warehouse_8017F59C[28] = {
#include "assets/dryfield_warehouse_animation_02260_bank4.inc"
};

AnimationRecord D_dryfield_warehouse_8017F60C[123] = {
#include "assets/dryfield_warehouse_animation_02260_records.inc"
};

u16 D_dryfield_warehouse_8017F7F8[20] = {
#include "assets/dryfield_warehouse_animation_02260_indices.inc"
};

GpAnimSet D_dryfield_warehouse_8017F820 = {
    D_dryfield_warehouse_8017F60C,
    D_dryfield_warehouse_8017F7F8,
    { NULL, D_dryfield_warehouse_8017F584, NULL, NULL, D_dryfield_warehouse_8017F59C, NULL, NULL, NULL },
};

GpAnimSet* D_dryfield_warehouse_8017F848[2] = {
    NULL,
    &D_dryfield_warehouse_8017F820,
};

GpXformArg D_dryfield_warehouse_8017F850 = { { 5540, 0, -2300, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_dryfield_warehouse_8017F868 = { { 4654, 0, -1968, 0 }, { 0, 512, 0, 0 } };

GpEvsCmd D_dryfield_warehouse_8017F880[16] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_warehouse_8017E3F4 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_warehouse_8017FA00[11] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_warehouse_8017DA58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_warehouse_8017DA58 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_warehouse_8017FB08[3] = {
    { 0, 192, func_dryfield_warehouse_8017E090, { .model = NULL } },
    { 0, 192, func_dryfield_warehouse_8017E308, { .model = NULL } },
    { 0, 192, func_dryfield_warehouse_8017E22C, { .model = NULL } },
};

SVECTOR D_dryfield_warehouse_8017FB2C[16] = {
    { 1190, -2950, -1540, 0 },
    { 310, -510, -180, 0 },
    { 2130, -2950, -3235, 0 },
    { 1200, -400, -1770, 0 },
    { 4340, -2950, -2940, 0 },
    { 3270, 0, -1290, 0 },
    { 3990, -2950, -1990, 0 },
    { 3030, -290, -510, 0 },
    { 5140, -2100, -3920, 0 },
    { 3860, -2100, -3920, 0 },
    { 3860, -660, -3920, 0 },
    { 5140, -660, -3920, 0 },
    { 4340, 0, -1930, 0 },
    { 3060, 0, -1930, 0 },
    { 3500, 0, -3030, 0 },
    { 4780, 0, -3030, 0 },
};

s16 D_dryfield_warehouse_8017FBAC[8] = {
    100,
    150,
    125,
    175,
    135,
    185,
    100,
    150,
};

GpRoomObjRec D_dryfield_warehouse_8017FBBC[3] = {
    { D_dryfield_warehouse_801802A8, D_dryfield_warehouse_801816A4, D_dryfield_warehouse_801817D4, NULL },
    { D_dryfield_warehouse_801809AC, D_dryfield_warehouse_801816A4, D_dryfield_warehouse_80181BB0, NULL },
    { D_dryfield_warehouse_80181038, D_dryfield_warehouse_801816A4, D_dryfield_warehouse_801817D4, NULL },
};

u8 D_dryfield_warehouse_8017FBEC[12] = {
    1,
    2,
    6,
    7,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

u8 D_dryfield_warehouse_8017FBF8[12] = {
    1,
    2,
    9,
    8,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

u8* D_dryfield_warehouse_8017FC04[3] = {
    D_8010CAF8,
    D_dryfield_warehouse_8017FBEC,
    D_dryfield_warehouse_8017FBF8,
};

GpViewCountRec D_dryfield_warehouse_8017FC10[3] = {
    { { .bytes = { 9, 0 } } },
    { { .bytes = { 9, 0 } } },
    { { .bytes = { 9, 0 } } },
};

GpRoomCoordRec D_dryfield_warehouse_8017FC18[3] = {
    { D_dryfield_warehouse_801820E8, NULL },
    { D_dryfield_warehouse_801820E8, NULL },
    { D_dryfield_warehouse_801820E8, NULL },
};

GpWarpRec D_dryfield_warehouse_8017FC30[2] = {
    { { .words = { 0, 2631, 0, -3535 } }, { 0, 0, 0, 0 }, { .words = { 1024, 1721, 0, -2640 } }, { 0, 0, 0, 0 }, 0x52070002, 0x52070001, 0, 2, 0, 468 },
    { { .words = { 3072, 5437, 2, -1084 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4856, 2, -2146 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 0, 0 },
};

SVECTOR D_dryfield_warehouse_8017FCA0[7] = {
#include "assets/dryfield_warehouse_collision_02CE8_normals.inc"
};

SVECTOR D_dryfield_warehouse_8017FCD8[90] = {
#include "assets/dryfield_warehouse_collision_02CE8_verts.inc"
};

GpGridFace D_dryfield_warehouse_8017FFA8[50] = {
#include "assets/dryfield_warehouse_collision_02CE8_faces.inc"
};

s16 D_dryfield_warehouse_80180200[80] = {
#include "assets/dryfield_warehouse_collision_02CE8_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_warehouse_80180200[i])
s16* D_dryfield_warehouse_801802A0[2] = {
#include "assets/dryfield_warehouse_collision_02CE8_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_warehouse_801802A8[1] = {
    { NULL, D_dryfield_warehouse_8017FCA0, D_dryfield_warehouse_8017FCD8, D_dryfield_warehouse_8017FFA8, D_dryfield_warehouse_801802A0, 0, 4000, 2, 1, 4000, 50 },
};

SVECTOR D_dryfield_warehouse_801802CC[7] = {
#include "assets/dryfield_warehouse_collision_033EC_normals.inc"
};

SVECTOR D_dryfield_warehouse_80180304[99] = {
#include "assets/dryfield_warehouse_collision_033EC_verts.inc"
};

GpGridFace D_dryfield_warehouse_8018061C[56] = {
#include "assets/dryfield_warehouse_collision_033EC_faces.inc"
};

s16 D_dryfield_warehouse_801808BC[112] = {
#include "assets/dryfield_warehouse_collision_033EC_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_warehouse_801808BC[i])
s16* D_dryfield_warehouse_8018099C[4] = {
#include "assets/dryfield_warehouse_collision_033EC_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_warehouse_801809AC[1] = {
    { NULL, D_dryfield_warehouse_801802CC, D_dryfield_warehouse_80180304, D_dryfield_warehouse_8018061C, D_dryfield_warehouse_8018099C, 0, 4100, 2, 2, 4000, 56 },
};

SVECTOR D_dryfield_warehouse_801809D0[7] = {
#include "assets/dryfield_warehouse_collision_03A78_normals.inc"
};

SVECTOR D_dryfield_warehouse_80180A08[91] = {
#include "assets/dryfield_warehouse_collision_03A78_verts.inc"
};

GpGridFace D_dryfield_warehouse_80180CE0[52] = {
#include "assets/dryfield_warehouse_collision_03A78_faces.inc"
};

s16 D_dryfield_warehouse_80180F50[108] = {
#include "assets/dryfield_warehouse_collision_03A78_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_warehouse_80180F50[i])
s16* D_dryfield_warehouse_80181028[4] = {
#include "assets/dryfield_warehouse_collision_03A78_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_warehouse_80181038[1] = {
    { NULL, D_dryfield_warehouse_801809D0, D_dryfield_warehouse_80180A08, D_dryfield_warehouse_80180CE0, D_dryfield_warehouse_80181028, 0, 4100, 2, 2, 4000, 52 },
};

GpViewRec D_dryfield_warehouse_8018105C[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3000, 7700, 2000 } }, 230 },
    { { { { -981, 0, 3976 }, { -168, 4092, -41 }, { -3972, -173, -981 } }, { -5338, 1066, 1266 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 2311, 0, -3381 }, { -2032, 3273, -1389 }, { 2702, 2461, 1847 } }, { -5038, 1666, 2016 } }, 207 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { 1114, 0, -3941 }, { -1359, 3844, -384 }, { 3699, 1413, 1046 } }, { -2338, 2116, 2216 } }, 257 },
    { { { { -832, 0, -4010 }, { -256, 4087, 53 }, { 4002, 262, -831 } }, { -288, 1366, 916 } }, 207 },
};

GpSprtCmd D_dryfield_warehouse_801811A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_warehouse_801811B0[21] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, 16, 1012, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, 24, 1025, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -112, 64, 450, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, 64, 425, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 56, 650, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 88, 480, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 399, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 64, 650, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 559, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 88, 32, 650, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 96, 64, 553, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 104, 80, 349, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 88, 481, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 346, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 32, 616, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, 32, 564, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 32, 521, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -160, 72, 612, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 96, 612, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -160, 0, 600, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -160, 56, 612, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_warehouse_80181354[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 2, 0, 0, { 0, 0 } },
    { 4, 13, 0, 0, { 2, 0 } },
    { 17, 4, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_warehouse_80181384[25] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 32, 962, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 32, 875, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 48, 32, 825, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 32, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 80, 600, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 96, 550, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -88, 96, 550, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 72, 550, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -88, 72, 550, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 72, 600, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 56, 625, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 56, 625, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 56, 700, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 40, 725, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 40, 725, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 40, 725, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 32, 725, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 24, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 24, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 32, 725, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 48, 625, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 120, 48, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 16, 625, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 120, 16, 625, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 0, 625, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_warehouse_80181578[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 1, 0, 0, { 0, 0 } },
    { 3, 17, 0, 0, { 2, 0 } },
    { 20, 5, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_warehouse_801815A8[2] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 80, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 96, 550, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_warehouse_801815D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_warehouse_801815E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

static void func_dryfield_warehouse_8017DBB0(Task* arg0);
static void func_dryfield_warehouse_8017E414(GfxCoord* coord, s16 arg1);
static void func_dryfield_warehouse_8017ED34(GfxCoord* coord, s16 arg1, s16 arg2);

/// Message handler of the warehouse's cutscene task. Message 0 re-opens the
/// room: it kills the screen-fade task still on `D_dryfield_warehouse_801821C0`,
/// turns the display back on and, while `DwhWork::playerEffActive` is up, ends
/// the weapon effect and re-sends the player-weapon record. The owner is then
/// handed that same 0x3E8 record -- `GpAnimArg::animBlock.index` is the equipped weapon's
/// animation id, `Player_Status.weapon` plus 1 or 0x22 depending on `Mc_SaveData[0].state.characterId`, with 1
/// and 0 padding it out -- followed by the room's placement as msg 0x3E9.
///
/// The session's weapon id is synced to 2 once, and `D_dryfield_warehouse_801821C4`
/// records whether this handler did that: message 1 mirrors the session's view
/// and object tables back onto that flag. The 1 shared by the record and the
/// flag is one callee-saved value because both outlive the dispatches.
void func_dryfield_warehouse_8017DA58(s32 arg0)
{
    DwhWork*  work;
    GpAnimArg rec;
    s32       weaponId;
    s32       anim;

    switch (arg0) {
        case 0:
            if (D_dryfield_warehouse_801821C0 != 0) {
                taskKill(D_dryfield_warehouse_801821C0);
            }
            SetDispMask(1);
            work = (DwhWork*)D_dryfield_warehouse_801821BC->work;
            if (work->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                work->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId            = Player_Status.weapon;
            anim                = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animBlock.index = anim;
            rec.field_4         = 1;
            rec.field_8         = 0;
            rec.field_C         = 0;
            rec.field_10        = 1;
            Gp_DispatchMsgPtr((Task*)work->owner, 0x3E8, &rec, 0);
            Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_warehouse_8017F868, 0);
            if (Mc_SaveData[0].state.at4.loc.room != 2) {
                Mc_SaveData[0].state.at4.loc.room = 2;
                gGameSession->at4.loc.room        = 2;
                D_dryfield_warehouse_801821C4     = 1;
                return;
            }
            D_dryfield_warehouse_801821C4 = 0;
            return;
        case 1:
            if (D_dryfield_warehouse_801821C4 != 0) {
                gGameSession->viewDirty     = arg0;
                gGameSession->roomObjsDirty = arg0;
            }
            return;
    }
}

/// Per-frame script step of the warehouse's cutscene task, dispatched on
/// `DwhWork::field_4` with `field_6` as the sub-step. States 1 and 5 advance a
/// frame counter in `field_E` and play a sound every 60 frames; 3 re-sends the
/// weapon record and spawns five staggered effects until `field_8` reaches 36;
/// 4 and 5 draw a white fade. State 2 spawns entry 1 of the room task table into
/// `D_dryfield_warehouse_801821C0`; it, state 0 and any unknown state reset
/// `field_4` to 0, as does state 3 once its timer runs out.
static void func_dryfield_warehouse_8017DBB0(Task* arg0)
{
    DwhWork* work;
    DwhWork* shared;
    DwhWork* cur;
    union {
        GpAnimArg rec;
        SVECTOR   pos;
    } msg;
    s32 weaponId;
    s32 anim;

    work = (DwhWork*)arg0->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            switch (work->field_6) {
                case 0:
                    SetDispMask(1);
                    Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    Gp_KillPlayerEffs();
                    work->playerEffActive = 1;
                    cur                   = (DwhWork*)arg0->work;
                    if (cur->owner != NULL) {
                        msg.rec.animBlock.ptr = D_dryfield_warehouse_8017F848;
                        msg.rec.field_4       = 1;
                        msg.rec.field_8       = 0;
                        msg.rec.field_C       = 0;
                        msg.rec.field_10      = 0;
                        Gp_DispatchMsgPtr((Task*)cur->owner, 0x3F4, &msg.rec, 0);
                    }
                    Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_warehouse_8017F850, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    break;
                case 1:
                    if ((work->field_E % 60) == 0) {
                        SndEvt_EnqueueType6(0x52070003, 0, 0);
                    }
                    break;
            }
            work->field_E++;
            return;
        case 2:
            D_dryfield_warehouse_801821C0 = Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 1, 8, 0);
            break;
        case 3:
            shared = (DwhWork*)D_dryfield_warehouse_801821BC->work;
            if (shared->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                shared->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId                = Player_Status.weapon;
            anim                    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.rec.animBlock.index = anim;
            msg.rec.field_4         = 1;
            msg.rec.field_8         = 0;
            msg.rec.field_C         = 0;
            msg.rec.field_10        = 1;
            /* The message ABI carries this object address in one 32-bit word. */
            Gp_DispatchMsg((Task*)shared->owner, 0x3E8, (s32)&msg.rec, 0);
            Gp_DispatchMsgPtr((Task*)shared->owner, 0x3E9, &D_dryfield_warehouse_8017F868, 0);
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    work->field_8++;
                    shared     = (DwhWork*)arg0->work;
                    msg.pos.vx = 0x1644;
                    msg.pos.vy = 0;
                    if (!(shared->field_8 & 7)) {
                        msg.pos.vz = -500;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 1) & 7)) {
                        msg.pos.vz = -700;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 2) & 7)) {
                        msg.pos.vz = -900;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 3) & 7)) {
                        msg.pos.vz = -1100;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 4) & 7)) {
                        msg.pos.vz = -1300;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (work->field_8 >= 36) {
                        work->field_4 = 0;
                    }
                    SetDispMask(1);
                    return;
            }
            break;
        case 4:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            switch (work->field_6) {
                case 0:
                    Mc_SaveData[0].state.at4.loc.room = 2;
                    gGameSession->at4.loc.room        = 2;
                    work->field_8                     = 0;
                    work->field_6++;
                    break;
                case 1:
                    gGameSession->viewDirty     = 1;
                    gGameSession->roomObjsDirty = 1;
                    work->field_6++;
                    break;
                case 2:
                    break;
            }
            if (work->field_8 == 10) {
                SndEvt_EnqueueType6(0x52070004, 0, 0);
            }
            work->field_8++;
            return;
        case 5:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            switch (work->field_6) {
                case 0:
                    D_80115768    = 0;
                    work->field_E = 0;
                    work->field_6++;
                    break;
                case 1:
                    if ((work->field_E % 60) == 0) {
                        SndEvt_EnqueueType6(0x52070003, 0, 0);
                    }
                    break;
            }
            work->field_E++;
            return;
    }
    work->field_4 = 0;
}

/// Main loop of the warehouse's cutscene task, the owner of the 0x10-byte
/// `DwhWork` block. State 0 arms the script once: a `Gp_StateC08.field_A` of 1 or a live
/// `gDisplayState.pendingMode` both mean the cutscene is already up, so it does nothing.
/// Otherwise it parks the zeroed work block in `Task::work` -- a failed
/// `Mem_Malloc` kills the task, but the record below is dispatched either way --
/// fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_warehouse_801821BC` so the room's script helpers reach that block.
///
/// The 0x3E8 record is rebuilt here rather than taken from its owner: `GpAnimArg`
/// field 0 is the equipped weapon's animation id, `Player_Status.weapon` plus 1 or 0x22
/// depending on `Mc_SaveData[0].state.characterId`, and 1 and 0 pad it out. It is dispatched to a
/// freshly fetched slot 3, not to the work block's owner.
///
/// State 0 then falls into state 1, which only steps the machine, so a task
/// entering at 1 runs the step alone. State 2 kills the task once the session
/// has torn down (`gGameSession->eventState`), otherwise runs the script.
void func_dryfield_warehouse_8017E090(Task* arg0)
{
    DwhWork*  work;
    GpAnimArg rec;
    s32       weaponId;
    s32       anim;

    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                work       = Mem_Malloc(0x10, false);
                arg0->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(arg0);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                   = gameGetPtrSlot(3);
                    D_dryfield_warehouse_801821BC = arg0;
                }
                weaponId            = Player_Status.weapon;
                anim                = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.animBlock.index = anim;
                rec.field_4         = 1;
                rec.field_8         = 0;
                rec.field_C         = 0;
                rec.field_10        = 0;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), 0x3E8, &rec, 0);
                D_dryfield_warehouse_801821C0 = NULL;
                D_80115768                    = 1;
                arg0->state                   = arg0->state + 1;
                case 1:
                    func_800E8634(D_dryfield_warehouse_8017F880, 0,
                                  D_dryfield_warehouse_8017FA00);
                    arg0->state = arg0->state + 1;
                    return;
            }
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            func_dryfield_warehouse_8017DBB0(arg0);
            break;
    }
}

/// Screen-fade task running the other way from `func_dryfield_warehouse_8017E308`:
/// on its first tick it allocates the 8-byte block and saturates all three
/// channels at 0xFF, then every frame it draws the fade overlay and lowers each
/// channel by `Task::spawnArg1`. Once `r` falls below 0 the screen is clear and
/// the task kills itself.
void func_dryfield_warehouse_8017E22C(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1.value);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1.value);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1.value);
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Screen-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block and seeds all three channels to 0, then every frame it draws the fade
/// overlay and steps each channel up by `Task::spawnArg1`. `r` is the one the
/// end-of-fade test watches, so once it has reached 0x100 the display is
/// switched back on, the room's fade-task handle is cleared and the task kills
/// itself.
void func_dryfield_warehouse_8017E308(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r + (u16)arg0->spawnArg1.value);
            fade->g = (s16)((u16)fade->g + (u16)arg0->spawnArg1.value);
            fade->b = (s16)((u16)fade->b + (u16)arg0->spawnArg1.value);
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            D_dryfield_warehouse_801821C0 = NULL;
            taskKill(arg0);
            break;
    }
}

/// Hands the warehouse's cutscene task the script step `arg0` to run,
/// starting from its first sub-step.
void func_dryfield_warehouse_8017E3F4(s16 arg0)
{
    DwhWork* work = (DwhWork*)D_dryfield_warehouse_801821BC->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}

/// Draws one prism from `D_dryfield_warehouse_8017FB2C[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring `[4..7]`,
/// then a cap over the lit ring. Each corner is rotated by `coord`'s `workm` and
/// moved by its translation before projection through `GsWSMATRIX`. The lit
/// corners share a grey of 0x18 plus a small pulse; the far corners are black.
static void func_dryfield_warehouse_8017E414(GfxCoord* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    u8               shade;

    SCRATCH_PUSH(RoomQuadScratch);
    blk = SCRATCH_HEAD(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
        blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
        blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
        blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
        blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
        blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
        blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
        blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
        blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
    blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
    blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
    blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
    blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
    blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
    blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_warehouse_8017FB2C[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
    blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
    blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, shade, shade, shade);
    setRGB1(prim, shade, shade, shade);
    setRGB2(prim, shade, shade, shade);
    setRGB3(prim, shade, shade, shade);
    addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    SCRATCH_POP(RoomQuadScratch);
}

/// Draws one ring of gouraud `POLY_G4` segments between two circles in the XZ
/// plane of `coord`: circle `arg1` of the room's centre/radius tables forms the
/// lit edge and circle `arg1 + 1` the black one. `arg2` segments cover the full
/// turn, starting at a phase that advances with the frame counter. Each corner
/// is placed in `coord`'s space through its `workm`, then projected through
/// `GsWSMATRIX`; the lit edge glows at 0x14 plus a small pulse.
static void func_dryfield_warehouse_8017ED34(GfxCoord* coord, s16 arg1, s16 arg2)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s16              level;
    s16              step;
    s16              start;
    s32              angle;
    s32              next;

    level = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x14;
    SCRATCH_PUSH(RoomQuadScratch);
    blk   = SCRATCH_HEAD(RoomQuadScratch);
    start = gDisplayState.animFrame & 0xFFF;
    step  = 0x1000 / arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (angle = start; angle < start + step * arg2; angle = next) {
        blk->v[0].vx = D_dryfield_warehouse_8017FB2C[arg1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->v[0].vy = D_dryfield_warehouse_8017FB2C[arg1].vy;
        blk->v[0].vz = D_dryfield_warehouse_8017FB2C[arg1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[0]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        next          = angle + step;
        blk->v[0].vz += coord->workm.t[2];

        blk->v[1].vx = D_dryfield_warehouse_8017FB2C[arg1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        blk->v[1].vy = D_dryfield_warehouse_8017FB2C[arg1].vy;
        blk->v[1].vz = D_dryfield_warehouse_8017FB2C[arg1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[1]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];

        blk->v[2].vx = D_dryfield_warehouse_8017FB2C[arg1 + 1].vx + ((rsin(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->v[2].vy = D_dryfield_warehouse_8017FB2C[arg1 + 1].vy;
        blk->v[2].vz = D_dryfield_warehouse_8017FB2C[arg1 + 1].vz + ((rcos(angle) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[2]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];

        blk->v[3].vx = D_dryfield_warehouse_8017FB2C[arg1 + 1].vx + ((rsin(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        blk->v[3].vy = D_dryfield_warehouse_8017FB2C[arg1 + 1].vy;
        blk->v[3].vz = D_dryfield_warehouse_8017FB2C[arg1 + 1].vz + ((rcos(next) * D_dryfield_warehouse_8017FBAC[arg1 + 1]) >> 12);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[3]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, level, level, level);
        setRGB1(prim, level, level, level);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    SCRATCH_POP(RoomQuadScratch);
}

/// Per-frame effect on the room's model task: re-poses the model for the
/// current stage visit, then publishes variant 2 as the room's
/// `Gp_State1C->roomEffectMode` index. `Task::extra` is the task's `TmdObject`, so
/// `field_8` is the coordinate every pose shares. The stage-visit byte
/// `gGameSession->at4.loc.view` is used as a bit index: bits 2, 3, 6 and 9 (`0x24C`)
/// pose through `func_dryfield_warehouse_8017E414`, bit 2 (`4`) also drives
/// `func_dryfield_warehouse_8017ED34` to step 0, those same `0x24C` visits also
/// drive it to step 2, and bits 2, 3, 4 and 6-9 (`0x3DC`) drive it to steps 4
/// and 6.
void func_dryfield_warehouse_8017F494(Task* arg0)
{
    s32       mask;
    s32       poseMask;
    GfxCoord* coord;

    mask     = 1 << gGameSession->at4.loc.view;
    poseMask = mask & 0x24C;
    coord    = arg0->extra.tmd->coords;
    if (poseMask != 0) {
        func_dryfield_warehouse_8017E414(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_warehouse_8017ED34(coord, 0, 8);
    }
    if (poseMask != 0) {
        func_dryfield_warehouse_8017ED34(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_warehouse_8017ED34(coord, 4, 8);
        func_dryfield_warehouse_8017ED34(coord, 6, 8);
    }
    Gp_State1C->roomEffectMode = 2;
}
