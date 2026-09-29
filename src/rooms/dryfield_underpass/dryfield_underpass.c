#include "rooms/dryfield_underpass.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"

extern TaskDesc   D_dryfield_underpass_8017E818[];
extern GpMsgEntry D_dryfield_underpass_8017E830[];
extern s32        D_dryfield_underpass_8017E89C;
extern GpEvsCmd   D_dryfield_underpass_8017E8D8[];
extern SVECTOR    D_dryfield_underpass_8017EAD0[8];
extern s16        D_dryfield_underpass_8017EB10[8];

extern GpGridParams   D_dryfield_underpass_8017F484[1];
extern GpObj3A        D_dryfield_underpass_80180AA8[3];
extern GpObj4C        D_dryfield_underpass_80180388[16];
extern GpObj4C        D_dryfield_underpass_80180848[2];
extern GpObj4C        D_dryfield_underpass_801808E0[6];
extern GpRoomCoordSet D_dryfield_underpass_80180EBC[1];
extern GpRoomCoordSet D_dryfield_underpass_80181114[1];

extern GpAnimArg D_dryfield_underpass_8017E870;
extern GpAnimArg D_dryfield_underpass_8017E884;
extern GpCmdArg  D_dryfield_underpass_8017E8A0;
extern GpCmdArg  D_dryfield_underpass_8017E8A4;
extern GpCmdArg  D_dryfield_underpass_8017E8A8;
extern GpCopyArg D_dryfield_underpass_8017E868;
void             func_dryfield_underpass_8017DA08(void);

s32  func_dryfield_underpass_8017D788(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_underpass_8017D868(Task*, s32, s32, GpMessageArg);
s32  func_dryfield_underpass_8017D8CC(Task*, s32, s32, s32);
s32  func_dryfield_underpass_8017D900(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_underpass_8017D908(Task*, s32, RoomEventMsg*, RoomEventMsg*);
void func_dryfield_underpass_8017D5D0(Task*);

GpPackedPose D_dryfield_underpass_8017DEE0[10] = {
#include "assets/dryfield_underpass_animation_00E64_bank1.inc"
};

AnimationPackedRotation D_dryfield_underpass_8017DF58[126] = {
#include "assets/dryfield_underpass_animation_00E64_bank4.inc"
};

GpAnimRec D_dryfield_underpass_8017E150[171] = {
#include "assets/dryfield_underpass_animation_00E64_records.inc"
};

u16 D_dryfield_underpass_8017E3FC[20] = {
#include "assets/dryfield_underpass_animation_00E64_indices.inc"
};

GpAnimSet D_dryfield_underpass_8017E424 = {
    D_dryfield_underpass_8017E150,
    D_dryfield_underpass_8017E3FC,
    { NULL, D_dryfield_underpass_8017DEE0, NULL, NULL, D_dryfield_underpass_8017DF58, NULL, NULL, NULL },
};

GpPackedPose D_dryfield_underpass_8017E44C[6] = {
#include "assets/dryfield_underpass_animation_01230_bank1.inc"
};

AnimationPackedRotation D_dryfield_underpass_8017E494[64] = {
#include "assets/dryfield_underpass_animation_01230_bank4.inc"
};

GpAnimRec D_dryfield_underpass_8017E594[141] = {
#include "assets/dryfield_underpass_animation_01230_records.inc"
};

u16 D_dryfield_underpass_8017E7C8[20] = {
#include "assets/dryfield_underpass_animation_01230_indices.inc"
};

GpAnimSet D_dryfield_underpass_8017E7F0 = {
    D_dryfield_underpass_8017E594,
    D_dryfield_underpass_8017E7C8,
    { NULL, D_dryfield_underpass_8017E44C, NULL, NULL, D_dryfield_underpass_8017E494, NULL, NULL, NULL },
};

TaskDesc D_dryfield_underpass_8017E818[2] = {
    { 0, 32, func_dryfield_underpass_8017D5D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_underpass_8017E830[6] = {
    { 5102, func_dryfield_underpass_8017D788 },
    { 5105, func_dryfield_underpass_8017D900 },
    { 5103, func_dryfield_underpass_8017D908 },
    { 5104, func_dryfield_underpass_8017D868 },
    { 5106, func_dryfield_underpass_8017D8CC },
    { 0x7FFFFFFF, NULL },
};

GpAnimSet* D_dryfield_underpass_8017E860[2] = {
    &D_dryfield_underpass_8017E424,
    &D_dryfield_underpass_8017E7F0,
};

GpCopyArg D_dryfield_underpass_8017E868 = { { .sets = D_dryfield_underpass_8017E860 }, 2 };

GpAnimArg D_dryfield_underpass_8017E870 = { { .index = 1 }, 47, 1, 8, 1 };

GpAnimArg D_dryfield_underpass_8017E884 = { { .index = 1 }, 48, 1, 8, 1 };

GpCmdArg D_dryfield_underpass_8017E898 = { { .loc = { 5, 11 } }, 0 };

s32 D_dryfield_underpass_8017E89C = 0x10B05;

GpCmdArg D_dryfield_underpass_8017E8A0 = { { .loc = { 5, 11 } }, 2 };

GpCmdArg D_dryfield_underpass_8017E8A4 = { { .loc = { 5, 11 } }, 3 };

GpCmdArg D_dryfield_underpass_8017E8A8 = { { .loc = { 5, 11 } }, 4 };

GpAnimArg D_dryfield_underpass_8017E8AC = { { .index = 1 }, 1, 0, 0, 1 };

GpXformArg D_dryfield_underpass_8017E8C0 = { { 0x3EE0, -1000, -3624, 0 }, { 0, -2048, 0, 0 } };

GpEvsCmd D_dryfield_underpass_8017E8D8[21] = {
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_underpass_8017E868 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_underpass_8017E884 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_underpass_8017E8A0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_underpass_8017E870 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_underpass_8017E8A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_underpass_8017E8C0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_dryfield_underpass_8017E8A8 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_underpass_8017DA08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_underpass_8017EAD0[8] = {
    { 0x4588, -3480, -1500, 0 },
    { 0x3B60, -3480, -6000, 0 },
    { 0x4588, -3480, -8700, 0 },
    { 0x34BC, -3480, -7200, 0 },
    { 9300, -3480, -9800, 0 },
    { 6200, -3480, -0x29CC, 0 },
    { 4850, -3420, -7200, 0 },
    { 500, -3480, -9700, 0 },
};

s16 D_dryfield_underpass_8017EB10[8] = {
    2052,
    8,
    536,
    528,
    16,
    64,
    32,
    160,
};

GpRoomObjRec D_dryfield_underpass_8017EB20[6] = {
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180388, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180848, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
    { D_dryfield_underpass_8017F484, D_dryfield_underpass_80180848, D_dryfield_underpass_801808E0, D_dryfield_underpass_80180AA8 },
};

u8 D_dryfield_underpass_8017EB80[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_underpass_8017EB8C[12] = {
    1,
    11,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_underpass_8017EB98[12] = {
    1,
    21,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

u8 D_dryfield_underpass_8017EBA4[12] = {
    1,
    22,
    23,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8 D_dryfield_underpass_8017EBB0[12] = {
    1,
    24,
    25,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8* D_dryfield_underpass_8017EBBC[6] = {
    D_8010CAF8,
    D_dryfield_underpass_8017EB80,
    D_dryfield_underpass_8017EB8C,
    D_dryfield_underpass_8017EB98,
    D_dryfield_underpass_8017EBA4,
    D_dryfield_underpass_8017EBB0,
};

GpViewCountRec D_dryfield_underpass_8017EBD4[6] = {
    { { .bytes = { 26, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
    { { .bytes = { 11, 0 } } },
};

GpRoomCoordRec D_dryfield_underpass_8017EBE0[6] = {
    { D_dryfield_underpass_80180EBC, NULL },
    { D_dryfield_underpass_80181114, NULL },
    { D_dryfield_underpass_80180EBC, NULL },
    { D_dryfield_underpass_80181114, NULL },
    { D_dryfield_underpass_80180EBC, NULL },
    { D_dryfield_underpass_80180EBC, NULL },
};

GpWarpRec D_dryfield_underpass_8017EC10[3] = {
    { { .words = { 0, 5303, -997, -0x2C94 } }, { 0, 0, 0, 0 }, { .words = { 0, 5303, -997, -0x2C94 } }, { 0, 0, 0, 0 }, 0x52260008, 0x52260007, 0, 6, 0, 463 },
    { { .words = { 1024, 0x3D31, -1000, -4216 } }, { 0, 0, 0, 0 }, { .words = { 1024, 0x3D31, -1000, -4216 } }, { 0, 0, 0, 0 }, 0x52260006, 0x52260005, 0, 2, 0, 462 },
    { { .words = { 2048, 1212, -1000, -7543 } }, { 0, 0, 0, 0 }, { .words = { 2048, 1212, -1000, -7543 } }, { 0, 0, 0, 0 }, 0x52260001, 0x52260001, 0, 7, 2, 0 },
};

SVECTOR D_dryfield_underpass_8017ECB8[12] = {
#include "assets/dryfield_underpass_collision_01EC4_normals.inc"
};

SVECTOR D_dryfield_underpass_8017ED18[70] = {
#include "assets/dryfield_underpass_collision_01EC4_verts.inc"
};

GpGridFace D_dryfield_underpass_8017EF48[46] = {
#include "assets/dryfield_underpass_collision_01EC4_faces.inc"
};

s16 D_dryfield_underpass_8017F170[346] = {
#include "assets/dryfield_underpass_collision_01EC4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_underpass_8017F170[i])
s16* D_dryfield_underpass_8017F424[24] = {
#include "assets/dryfield_underpass_collision_01EC4_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_underpass_8017F484[1] = {
    { NULL, D_dryfield_underpass_8017ECB8, D_dryfield_underpass_8017ED18, D_dryfield_underpass_8017EF48, D_dryfield_underpass_8017F424, 3000, 0x32C8, 6, 4, 4000, 46 },
};

GpViewRec D_dryfield_underpass_8017F4A8[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7864, 0x6671, 5100 } }, 329 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { -4047, 0, 631 }, { -451, 2858, -2898 }, { -440, -2933, -2824 } }, { -0x4229, 2000, 6853 } }, 348 },
};

GpAreaTmdRec D_dryfield_underpass_8017F850[2] = {
    { 5, 5, 3, 0, { 0, 0 }, D_80153D60 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_underpass_8017F868[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017BCC4, D_dryfield_underpass_8017F850 },
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

GpSprtCmd D_dryfield_underpass_8017F8D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017F8E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017F8F0[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_8017FA30[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017FA48[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_8017FB24[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017FB3C[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_8017FC04[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017FC24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017FC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017FC44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017FC54[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_8017FD30[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017FD50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017FD60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_8017FD70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017FD80[16] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_8017FEC0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017FED8[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_8017FFB4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_8017FFCC[10] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_80180094[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_801800B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_801800C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_801800D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_underpass_801800E4[11] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_underpass_801801C0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_801801E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_801801F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_80180200[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_80180210[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_80180220[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_80180230[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_underpass_80180240[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_underpass_80180250[26] = {
    { { .empty = D_dryfield_underpass_8017F8D0 }, D_dryfield_underpass_8017F8D0, NULL },
    { { .empty = D_dryfield_underpass_8017F8E0 }, D_dryfield_underpass_8017F8E0, NULL },
    { { .elements = D_dryfield_underpass_8017F8F0 }, D_dryfield_underpass_8017FA30, NULL },
    { { .elements = D_dryfield_underpass_8017FA48 }, D_dryfield_underpass_8017FB24, NULL },
    { { .elements = D_dryfield_underpass_8017FB3C }, D_dryfield_underpass_8017FC04, NULL },
    { { .empty = D_dryfield_underpass_8017FC24 }, D_dryfield_underpass_8017FC24, NULL },
    { { .empty = D_dryfield_underpass_8017FC34 }, D_dryfield_underpass_8017FC34, NULL },
    { { .empty = D_dryfield_underpass_8017FC44 }, D_dryfield_underpass_8017FC44, NULL },
    { { .elements = D_dryfield_underpass_8017FC54 }, D_dryfield_underpass_8017FD30, NULL },
    { { .empty = D_dryfield_underpass_8017FD50 }, D_dryfield_underpass_8017FD50, NULL },
    { { .empty = D_dryfield_underpass_8017FD60 }, D_dryfield_underpass_8017FD60, NULL },
    { { .empty = D_dryfield_underpass_8017FD70 }, D_dryfield_underpass_8017FD70, NULL },
    { { .elements = D_dryfield_underpass_8017FD80 }, D_dryfield_underpass_8017FEC0, NULL },
    { { .elements = D_dryfield_underpass_8017FED8 }, D_dryfield_underpass_8017FFB4, NULL },
    { { .elements = D_dryfield_underpass_8017FFCC }, D_dryfield_underpass_80180094, NULL },
    { { .empty = D_dryfield_underpass_801800B4 }, D_dryfield_underpass_801800B4, NULL },
    { { .empty = D_dryfield_underpass_801800C4 }, D_dryfield_underpass_801800C4, NULL },
    { { .empty = D_dryfield_underpass_801800D4 }, D_dryfield_underpass_801800D4, NULL },
    { { .elements = D_dryfield_underpass_801800E4 }, D_dryfield_underpass_801801C0, NULL },
    { { .empty = D_dryfield_underpass_801801E0 }, D_dryfield_underpass_801801E0, NULL },
    { { .empty = D_dryfield_underpass_801801F0 }, D_dryfield_underpass_801801F0, NULL },
    { { .empty = D_dryfield_underpass_80180200 }, D_dryfield_underpass_80180200, NULL },
    { { .empty = D_dryfield_underpass_80180210 }, D_dryfield_underpass_80180210, NULL },
    { { .empty = D_dryfield_underpass_80180220 }, D_dryfield_underpass_80180220, NULL },
    { { .empty = D_dryfield_underpass_80180230 }, D_dryfield_underpass_80180230, NULL },
    { { .empty = D_dryfield_underpass_80180240 }, D_dryfield_underpass_80180240, NULL },
};

GpObj4C D_dryfield_underpass_80180388[16] = {
    { NULL, NULL, NULL, { 0x4070, -2560, -5280, 0 }, { { -1616, -1888, 0, 0 }, { 1616, -1888, 0, 0 }, { -1616, 1888, 0, 0 }, { 1616, 1888, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2544, -5408, 0 }, { { 1616, -1840, 0, 0 }, { -1616, -1840, 0, 0 }, { 1616, 1840, 0, 0 }, { -1616, 1840, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x3A3F, -2720, -8402, 0 }, { { 65, -1824, -1727, 0 }, { -64, -1824, 1728, 0 }, { 65, 1824, -1727, 0 }, { -64, 1824, 1728, 0 } }, { 4101, 0, 151, 0 }, { 0, 0, 4096, 0 }, 2508, 0, 3, 9, 1, 0 },
    { NULL, NULL, NULL, { 0x3ABE, -2736, -8307, 0 }, { { -81, -1808, 1758, 0 }, { 81, -1808, -1757, 0 }, { -81, 1808, 1758, 0 }, { 81, 1808, -1757, 0 } }, { -4096, 0, -190, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 9, 3, 1, 0 },
    { NULL, NULL, NULL, { 9023, -2688, -8449, 0 }, { { 0, -1824, 1615, 0 }, { 1, -1824, -1616, 0 }, { 0, 1824, 1615, 0 }, { 1, 1824, -1616, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 8799, -2736, -8577, 0 }, { { -79, -1808, -1614, 0 }, { 78, -1808, 1613, 0 }, { -79, 1808, -1614, 0 }, { 78, 1808, 1613, 0 } }, { 4091, 0, -200, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 5214, -2656, -0x27A2, 0 }, { { 1616, -1824, 1, 0 }, { -1615, -1824, 0, 0 }, { 1616, 1824, 1, 0 }, { -1615, 1824, 0, 0 } }, { -2, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 5150, -2624, -9954, 0 }, { { -1615, -1888, 0, 0 }, { 1616, -1888, 1, 0 }, { -1615, 1888, 0, 0 }, { 1616, 1888, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 3230, -2624, -8482, 0 }, { { -79, -1792, -2412, 0 }, { 79, -1792, 2413, 0 }, { -79, 1792, -2412, 0 }, { 79, 1792, 2413, 0 } }, { 4094, 0, -135, 0 }, { 0, 0, 4096, 0 }, 2996, 0, 5, 7, 1, 0 },
    { NULL, NULL, NULL, { 3422, -2624, -8513, 0 }, { { 63, -1856, 2224, 0 }, { -63, -1856, -2223, 0 }, { 63, 1856, 2224, 0 }, { -63, 1856, -2223, 0 } }, { -4094, 0, 115, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 7, 5, 1, 0 },
    { NULL, NULL, NULL, { -960, -2592, -8577, 0 }, { { 306, -1856, 1579, 0 }, { -322, -1856, -1589, 0 }, { 306, 1856, 1579, 0 }, { -322, 1856, -1589, 0 } }, { -4028, 0, 798, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { -1056, -2528, -8385, 0 }, { { -322, -1856, -1592, 0 }, { 308, -1856, 1578, 0 }, { -322, 1856, -1592, 0 }, { 308, 1856, 1578, 0 } }, { 4028, 0, -802, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x3130, -2592, -8513, 0 }, { { 17, -1824, 1905, 0 }, { -16, -1824, -1904, 0 }, { 17, 1824, 1905, 0 }, { -16, 1824, -1904, 0 } }, { -4104, 0, 35, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 4, 9, 1, 0 },
    { NULL, NULL, NULL, { 0x3061, -2528, -8577, 0 }, { { 1, -1824, -1615, 0 }, { 0, -1824, 1616, 0 }, { 1, 1824, -1615, 0 }, { 0, 1824, 1616, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 9, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2624, -1029, 0 }, { { 1577, -1840, 182, 0 }, { -1630, -1840, -237, 0 }, { 1577, 1840, 182, 0 }, { -1630, 1840, -237, 0 } }, { -534, 0, 4073, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 10, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x40F1, -2624, -962, 0 }, { { -1854, -1840, -209, 0 }, { 1834, -1840, 184, 0 }, { -1854, 1840, -209, 0 }, { 1834, 1840, 184, 0 } }, { 434, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 10, 129, 0 },
};

GpObj4C D_dryfield_underpass_80180848[2] = {
    { NULL, NULL, NULL, { 0x4090, -2560, -2752, 0 }, { { 2096, -1888, 0, 0 }, { -2096, -1888, 0, 0 }, { 2096, 1888, 0, 0 }, { -2096, 1888, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x4090, -2544, -2656, 0 }, { { -2144, -1840, 0, 0 }, { 2144, -1840, 0, 0 }, { -2144, 1840, 0, 0 }, { 2144, 1840, 0, 0 } }, { 0, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, 129, 0 },
};

GpObj4C D_dryfield_underpass_801808E0[6] = {
    { NULL, NULL, NULL, { 5184, -1088, -0x2C70, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1108, 0, 34, 17, 2, 0 },
    { NULL, NULL, NULL, { 1280, -1056, -7456, 0 }, { { -896, 0, -432, 0 }, { 896, 0, -432, 0 }, { -896, 0, 432, 0 }, { 896, 0, 432, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 993, 0, 3, 51, 2, 0 },
    { NULL, NULL, NULL, { 0x3C00, -1088, -4096, 0 }, { { 368, 0, -928, 0 }, { 368, 0, 928, 0 }, { -368, 0, -928, 0 }, { -368, 0, 928, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 997, 0, 32, 34, 2, 0 },
    { NULL, NULL, NULL, { -1522, -1056, -8433, 0 }, { { -431, 0, -831, 0 }, { 433, 0, -832, 0 }, { -431, 0, 832, 0 }, { 432, 0, 832, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 936, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 0x40E0, -1056, -576, 0 }, { { -1024, 0, -304, 0 }, { 1024, 0, -304, 0 }, { -1024, 0, 304, 0 }, { 1024, 0, 304, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1063, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 0x40C0, -1056, -6240, 0 }, { { 1808, 0, -384, 0 }, { 1808, 0, 384, 0 }, { -1808, 0, -384, 0 }, { -1808, 0, 384, 0 } }, { 0, 4099, 0, 0 }, { 3166, 0, -2598, 0 }, 1846, 0x8005, 1, 0, 131, 0 },
};

GpObj3A D_dryfield_underpass_80180AA8[3] = {
    { NULL, NULL, { 1184, -2304, -0x2840, 0 }, { { -2912, -3328, 0, 0 }, { 2912, -3328, 0, 0 }, { -2912, 3328, 0, 0 }, { 2912, 3328, 0, 0 } }, { 0, 0, -4106, 0 }, { 52, 17 }, 1, 0 },
    { NULL, NULL, { 0x2FB0, -2240, -0x2820, 0 }, { { -5840, -3264, 0, 0 }, { 5840, -3264, 0, 0 }, { -5840, 3264, 0, 0 }, { 5840, 3264, 0, 0 } }, { 0, 0, -4111, 0 }, { 19, 26 }, 1, 0 },
    { NULL, NULL, { 8992, -3328, -4416, 0 }, { { -5840, -2848, 2496, 0 }, { 5840, -2848, -2496, 0 }, { -5840, 2848, 2496, 0 }, { 5840, 2848, -2496, 0 } }, { -1614, 0, -3777, 0 }, { 33, 27 }, 129, 0 },
};

GpPointLight D_dryfield_underpass_80180B5C[9] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3214, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1966, 2621, 3276, { 0, 0 } }, 2500, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3C28, -3483, -6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44C0, -3483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9300, -3483, -9600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BC, -3483, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6200, -3483, -0x2904 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -3483, -9500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4850, -3423, -7400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 3440, 2457, { 0, 0 } }, 2000, 4000 },
};

GpRoomCoordSet D_dryfield_underpass_80180EBC[1] = {
    { 0, NULL, 9, D_dryfield_underpass_80180B5C, 0, NULL },
};

GpPointLight D_dryfield_underpass_80180ED4[6] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4268, -2483, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -2483, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3F48, -2483, -8000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2AF8, -2483, -8300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5850, -2423, -9100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1720, 2088, 2457, { 0, 0 } }, 0, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x314C, -1973, -3920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 4096, 4096, { 0, 0 } }, 0, 8000 },
};

GpRoomCoordSet D_dryfield_underpass_80181114[1] = {
    { 0, NULL, 6, D_dryfield_underpass_80180ED4, 0, NULL },
};

s32 D_dryfield_underpass_8018112C[3] = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

s32 D_dryfield_underpass_80181138[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

GpRoomParamRec D_dryfield_underpass_80181144[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_underpass_8018114C[1] = {
    { 0, 0, 1, 0, D_dryfield_underpass_8018112C },
};

GpRoomParamRec D_dryfield_underpass_80181154[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_underpass_8018115C[1] = {
    { 0, 0, 1, 0, D_dryfield_underpass_80181138 },
};

GpRoomParamRec* D_dryfield_underpass_80181164[8] = {
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181154,
    D_dryfield_underpass_8018115C,
    D_dryfield_underpass_8018114C,
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181144,
    D_dryfield_underpass_80181144,
};

static void func_dryfield_underpass_8017D970(Task* arg0);
static void func_dryfield_underpass_8017DA00(Task* task);
static void func_dryfield_underpass_8017DB20(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

/// Switch task the room's 0x13F0 handler spawns: plays cap command `spawnArg2`,
/// waits for it to finish, and once its event key reaches 0xA toggles game
/// nibble `spawnArg1`. When that nibble is 0x51 it also picks the room variant
/// to load next from nibbles 0xC9, 0x53 and 0x51 and writes it to both the
/// session and the save data. The last state flags the view dirty when the
/// chosen room is 5 or above, then kills the task.
void func_dryfield_underpass_8017D5D0(Task* task)
{
    GpSaveLoc    src;
    GpSaveLoc    dst;
    GpSaveLoc*   s;
    GpSaveLoc*   d;
    GameSession* session;
    s32          flag;
    s32          state;
    s32          arg;
    u8           room;

    flag  = task->spawnArg1.value;
    state = task->state;
    arg   = task->spawnArg2.value;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
                if (flag == 0x51) {
                    d           = &dst;
                    s           = &src;
                    *(u16*)&src = 0x26;
                    src.field_5 = 0;
                    if (s->field_5 == 0) {
                        if (GameFlag_GetNibble(0xC9) != 0) {
                            if (GameFlag_GetNibble(0x53) != 0) {
                                d->field_3 = 2;
                            } else {
                                d->field_3 = 1;
                            }
                            if (GameFlag_GetNibble(0x51) == 0) {
                                dst.field_3 = dst.field_3 + 2;
                            }
                        } else {
                            if (GameFlag_GetNibble(0x51) != 0) {
                                d->field_3 = 5;
                            } else {
                                d->field_3 = 6;
                            }
                        }
                    }
                    session                           = gGameSession;
                    room                              = dst.field_3;
                    session->at4.loc.room             = room;
                    Mc_SaveData[0].state.at4.loc.room = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (gGameSession->at4.loc.room >= 5) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13EE: copies the incoming record onto the outgoing one
/// and, unless the query is report-only (`field_5` set), answers record id 0x20
/// with 1 or 2 from nibble 0x51, raised by 2 while nibble 0x53 is set, and
/// record id 0x22 with 1 or 2 from nibble 0x52. Always returns 1.
s32 func_dryfield_underpass_8017D788(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->prefix.packed == 0x20 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 += 2;
        }
    }
    if (in->prefix.packed == 0x22 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x52) == 0) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
    }
    return 1;
}

/// Handler for message 0x13F0: for `arg2` 1 or 2, spawns the room's switch
/// task `func_dryfield_underpass_8017D5D0` from the task table, toggling
/// nibble 0x51 with cap command 1 or nibble 0x52 with cap command 2. Any other
/// value spawns nothing. Always returns 0.
s32 func_dryfield_underpass_8017D868(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 1:
            Task_SpawnFromTable(D_dryfield_underpass_8017E818, 0, 0x51, 1);
            break;
        case 2:
            Task_SpawnFromTable(D_dryfield_underpass_8017E818, 0, 0x52, 2);
            break;
    }
    return 0;
}

/// Handler for message 0x13F2: when `arg2` is 2, queues stage sound 0x52260002.
/// Always returns 0.
s32 func_dryfield_underpass_8017D8CC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_EnqueueStageSnd6(0x52260000 | 2, 0, 0);
    }
    return 0;
}

/// Handler for message 0x13F1: does nothing and returns 0.
s32 func_dryfield_underpass_8017D900(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF: the first time record `field_2` 1 arrives while
/// the session's place is 1 and nibble 0xC9 is clear, sets that nibble and
/// runs the room's script `D_dryfield_underpass_8017E8D8`. Always returns 0.
s32 func_dryfield_underpass_8017D908(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_v1;

    temp_v1 = in->field_2;
    if ((temp_v1 == 1) && (gGameSession->at4.loc.place == temp_v1) && (GameFlag_GetNibble(0xC9) == 0)) {
        GameFlag_SetNibble(0xC9, 1);
        func_800E8614(D_dryfield_underpass_8017E8D8, 0);
    }
    return 0;
}

/// First state of the room task: parks the room's message table in
/// `Task::msgTable` and publishes the task in pointer slot 7. While the
/// session's place is 1 and nibble 0xC9 is clear it also sends message 0x7DA,
/// with `D_dryfield_underpass_8017E89C`, to the task in slot 4. Then advances.
static void func_dryfield_underpass_8017D970(Task* arg0)
{
    arg0->msgTable = D_dryfield_underpass_8017E830;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0xC9) == 0)) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &D_dryfield_underpass_8017E89C, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// Second state of the room task: idles.
static void func_dryfield_underpass_8017DA00(Task* task)
{
}

/// Picks the room variant to load next from nibbles 0xC9, 0x53 and 0x51, the
/// same choice the switch task `func_dryfield_underpass_8017D5D0` makes when it
/// toggles nibble 0x51, and writes it to the session's room and to
/// `Mc_SaveData[0].state.at4.loc.room`, then flags the room objects dirty. Reached from the room's
/// script data.
void func_dryfield_underpass_8017DA08(void)
{
    RoomEventMsg  src;
    RoomEventMsg  dst;
    RoomEventMsg* s;
    RoomEventMsg* d;
    GameSession*  session;
    u8            room;

    d           = &dst;
    s           = &src;
    *(u16*)&src = 0x26;
    src.field_5 = 0;
    if (s->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                d->field_3 = 2;
            } else {
                d->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                dst.field_3 = dst.field_3 + 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                d->field_3 = 5;
            } else {
                d->field_3 = 6;
            }
        }
    }
    session                           = gGameSession;
    room                              = dst.field_3;
    session->at4.loc.room             = room;
    Mc_SaveData[0].state.at4.loc.room = room;
    gGameSession->roomObjsDirty       = 1;
}

/// State handlers of the room task `func_dryfield_underpass_8017DAC8`, indexed
/// by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_underpass_8017D5C4 = {
    { func_dryfield_underpass_8017D970, func_dryfield_underpass_8017DA00, taskKill },
};

/// Room task: runs the state handler `D_dryfield_underpass_8017D5C4` names for
/// `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_underpass_8017DAC8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_underpass_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws one glow sprite at `arg1`, a point in the local space of the model
/// coordinate `arg0`: the point is rotated by the coordinate's `workm` and
/// offset by its translation, then projected through `GsWSMATRIX` in 0x14
/// bytes of scratch, released on exit. Anything nearer than `otz` 0x11 is
/// dropped. The sprite is a semi-transparent `POLY_FT4` on tpage 0x2B; `arg2`
/// picks the 40-texel-wide frame (u `arg2 * 40` onwards) and the clut
/// `(arg2 & 0x3F) | 0x4380`, and `arg3` is a signed half-extent, so the quad
/// reaches `(s16)arg3 * 39 / otz` from the projected centre. The grey level
/// alternates between 0x20 and 0x30 with `animFrame`.
static void func_dryfield_underpass_8017DB20(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    POLY_FT4*         prim;
    DisplayState*     ds;
    s32               su;
    s32               sv;
    s32               u0;
    s32               u1;
    s32               flip;
    s32               rgb;
    s16               xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomShaftScratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomShaftScratch*)(head - 0x14))->vec);
    block->vec.vx = (u16)block->vec.vx + (u16)arg0->workm.t[0];
    block->vec.vy = (u16)block->vec.vy + (u16)arg0->workm.t[1];
    block->vec.vz = (u16)block->vec.vz + (u16)arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        ds          = &gDisplayState;
        flip        = (u8)ds->animFrame;
        su          = (s16)arg2;
        sv          = (s16)arg3;
        prim->tpage = 0x2B;
        prim->clut  = (su & 0x3F) | 0x4380;
        u0          = su * 0x28;
        u1          = u0 + 0x27;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->v0    = 0;
        prim->v1    = 0;
        prim->v2    = 0x27;
        prim->v3    = 0x27;
        rgb         = (flip & 1) << 4;
        rgb        += 0x20;
        setSemiTrans(prim, 1);
        prim->r0         = rgb;
        prim->g0         = rgb;
        prim->b0         = rgb;
        block->halfWidth = (sv * 0x27) / block->otz;
        xy               = block->sx - (u16)block->halfWidth;
        prim->x2         = xy;
        prim->x0         = xy;
        xy               = block->sx + (u16)block->halfWidth;
        prim->x3         = xy;
        prim->x1         = xy;
        xy               = block->sy - (u16)block->halfWidth;
        prim->y1         = xy;
        prim->y0         = xy;
        xy               = block->sy + (u16)block->halfWidth;
        prim->y3         = xy;
        prim->y2         = xy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Per-frame effect on a model task: draws the glow sprites the current visit
/// lights, one per point in `D_...EAD0` (in the model's local space) whose
/// `D_...EB10` bitmask contains the visit's bit (`gGameSession->at4.loc.view`).
/// The whole effect is skipped unless nibble 0x53 is clear.
void func_dryfield_underpass_8017DE30(Task* task)
{
    GpCoord* coord;
    s32      mask;
    s32      i;
    SVECTOR* vec;
    s16*     flags;

    coord = task->extra.tmd->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x53) == 0) {
        i     = 0;
        vec   = D_dryfield_underpass_8017EAD0;
        flags = D_dryfield_underpass_8017EB10;
        do {
            if (mask & *flags) {
                func_dryfield_underpass_8017DB20(coord, vec, 0, 0x280);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
