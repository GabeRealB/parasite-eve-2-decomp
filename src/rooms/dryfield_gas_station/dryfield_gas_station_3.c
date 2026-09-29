#include "rooms/dryfield_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_gas_station_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#define D_dryfield_gas_station_80182E5C (D_dryfield_gas_station_80182E44[1])
#define D_dryfield_gas_station_80182E74 (D_dryfield_gas_station_80182E44[2])

/// Work block for the gas-station cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_gas_station_801807E0` and hung off `Task::work` (0x1C).
///
/// `owner` is the slot-3 game pointer (`gameGetPtrSlot(3)`) the task dispatches
/// its messages to, and `playerEffActive` is the flag guarding
/// `Gp_KillPlayerEffs` / `Gp_SpawnWeaponEff`. `field_4` is the script command
/// `func_dryfield_gas_station_801803C0` carries out and clears once it is done,
/// `field_6` the step within a multi-frame command (both written together by
/// `func_dryfield_gas_station_80180B2C`), and `field_8` the frame counter of the
/// command that walks the owner across the forecourt.
typedef struct DgsWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ u16   field_4;
    /* 0x06 */ u16   field_6;
    /* 0x08 */ u16   field_8;
    /* 0x0A */ byte  pad_A[0x2];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DgsWork;
STATIC_ASSERT_SIZEOF(DgsWork, 0x10);

extern GpAnimSet* D_dryfield_gas_station_80182E30[5];
extern GpEvsCmd   D_dryfield_gas_station_80182E8C[];
extern GpEvsCmd   D_dryfield_gas_station_8018303C[];

extern SVECTOR D_dryfield_gas_station_80183144;

/// The cutscene task `func_dryfield_gas_station_801807E0` publishes once its
/// `DgsWork` block is set up, so the room's script helpers can reach it.
extern Task* D_dryfield_gas_station_80184BD4;

// Indexed views below share one contiguous table.
void func_dryfield_gas_station_80180944(void);
void func_dryfield_gas_station_80180B2C(s16);

extern GpGridParams   D_dryfield_gas_station_80183EA4[1];
extern GpObj4C        D_dryfield_gas_station_80184350[11];
extern GpObj4C        D_dryfield_gas_station_80184694[10];
extern GpRoomCoordSet D_dryfield_gas_station_80184B48[1];
extern TaskDesc       D_80142604;
extern TaskDesc       D_8014D8A4;
void                  func_dryfield_gas_station_801807E0(Task*);
void                  func_dryfield_gas_station_80180984(Task*);
void                  func_dryfield_gas_station_80180A60(void);

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} DryfieldGasStationPoseBank48E0;

TaskDesc D_dryfield_gas_station_80181E7C[3] = {
    { 0, 192, func_dryfield_gas_station_801802C0, { .model = NULL } },
    { 0, 192, func_dryfield_gas_station_8017FFE4, { .model = NULL } },
    { 0, 192, func_dryfield_gas_station_801801E4, { .model = NULL } },
};

DryfieldGasStationPoseBank48E0 D_dryfield_gas_station_80181EA0 = { .poses = {
#include "assets/dryfield_gas_station_animation_04A70_bank1.inc"
} };

GpPackedSvec D_dryfield_gas_station_80181EB8[8] = {
#include "assets/dryfield_gas_station_animation_04A70_bank4.inc"
};

GpAnimRec D_dryfield_gas_station_80181ED8[76] = {
#include "assets/dryfield_gas_station_animation_04A70_records.inc"
};

u16 D_dryfield_gas_station_80182008[20] = {
#include "assets/dryfield_gas_station_animation_04A70_indices.inc"
};

GpAnimSet D_dryfield_gas_station_80182030 = {
    D_dryfield_gas_station_80181ED8, D_dryfield_gas_station_80182008,
    { NULL, D_dryfield_gas_station_80181EA0.words, NULL, NULL, D_dryfield_gas_station_80181EB8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[13];
    GpPackedSvec words[39];
} DryfieldGasStationPoseBank4A98;

DryfieldGasStationPoseBank4A98 D_dryfield_gas_station_80182058 = { .poses = {
#include "assets/dryfield_gas_station_animation_05210_bank1.inc"
} };

GpPackedSvec D_dryfield_gas_station_801820F4[179] = {
#include "assets/dryfield_gas_station_animation_05210_bank4.inc"
};

GpAnimRec D_dryfield_gas_station_801823C0[250] = {
#include "assets/dryfield_gas_station_animation_05210_records.inc"
};

u16 D_dryfield_gas_station_801827A8[20] = {
#include "assets/dryfield_gas_station_animation_05210_indices.inc"
};

GpAnimSet D_dryfield_gas_station_801827D0 = {
    D_dryfield_gas_station_801823C0, D_dryfield_gas_station_801827A8,
    { NULL, D_dryfield_gas_station_80182058.words, NULL, NULL, D_dryfield_gas_station_801820F4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[7];
    GpPackedSvec words[21];
} DryfieldGasStationPoseBank5238;

DryfieldGasStationPoseBank5238 D_dryfield_gas_station_801827F8 = { .poses = {
#include "assets/dryfield_gas_station_animation_055A4_bank1.inc"
} };

GpPackedSvec D_dryfield_gas_station_8018284C[65] = {
#include "assets/dryfield_gas_station_animation_055A4_bank4.inc"
};

GpAnimRec D_dryfield_gas_station_80182950[123] = {
#include "assets/dryfield_gas_station_animation_055A4_records.inc"
};

u16 D_dryfield_gas_station_80182B3C[20] = {
#include "assets/dryfield_gas_station_animation_055A4_indices.inc"
};

GpAnimSet D_dryfield_gas_station_80182B64 = {
    D_dryfield_gas_station_80182950, D_dryfield_gas_station_80182B3C,
    { NULL, D_dryfield_gas_station_801827F8.words, NULL, NULL, D_dryfield_gas_station_8018284C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[2];
    GpPackedSvec words[6];
} DryfieldGasStationPoseBank55CC;

DryfieldGasStationPoseBank55CC D_dryfield_gas_station_80182B8C = { .poses = {
#include "assets/dryfield_gas_station_animation_05848_bank1.inc"
} };

GpPackedSvec D_dryfield_gas_station_80182BA4[48] = {
#include "assets/dryfield_gas_station_animation_05848_bank4.inc"
};

GpAnimRec D_dryfield_gas_station_80182C64[95] = {
#include "assets/dryfield_gas_station_animation_05848_records.inc"
};

u16 D_dryfield_gas_station_80182DE0[20] = {
#include "assets/dryfield_gas_station_animation_05848_indices.inc"
};

GpAnimSet D_dryfield_gas_station_80182E08 = {
    D_dryfield_gas_station_80182C64, D_dryfield_gas_station_80182DE0,
    { NULL, D_dryfield_gas_station_80182B8C.words, NULL, NULL, D_dryfield_gas_station_80182BA4, NULL, NULL, NULL },
};

GpAnimSet * D_dryfield_gas_station_80182E30[5] = {
    &D_dryfield_gas_station_80182030,
    &D_dryfield_gas_station_80182B64,
    &D_dryfield_gas_station_80182E08,
    &D_dryfield_gas_station_801827D0,
    NULL,
};

GpXformArg D_dryfield_gas_station_80182E44[3] = {
    { { 14408, 0, -2630, 0 }, { 0, 3584, 0, 0 } },
    { { 14158, 0, -2380, 0 }, { 0, 2560, 0, 0 } },
    { { 14158, 0, -2380, 0 }, { 0, 3072, 0, 0 } },
};

GpEvsCmd D_dryfield_gas_station_80182E8C[18] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_gas_station_80180944 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_gas_station_8018303C[10] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_gas_station_80180A60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_gas_station_8018312C[2] = {
    { 0, 192, func_dryfield_gas_station_801807E0, { .model = NULL } },
    { 0, 192, func_dryfield_gas_station_80180984, { .model = NULL } },
};

SVECTOR D_dryfield_gas_station_80183144 = { 4378, -1383, -215, 0 };

GpRoomObjRec D_dryfield_gas_station_8018314C[1] = {
    { D_dryfield_gas_station_80183EA4, D_dryfield_gas_station_80184350, D_dryfield_gas_station_80184694, NULL },
};

u8 * D_dryfield_gas_station_8018315C[1] = {
    D_8010CAF8,
};

GpRoomCoordRec D_dryfield_gas_station_80183160[1] = {
    { D_dryfield_gas_station_80184B48, NULL },
};

GpViewCountRec D_dryfield_gas_station_80183168[1] = {
    { { .bytes = { 14, 0 } } },
};

GpWarpRec D_dryfield_gas_station_8018316C[3] = {
    { { .words = { 2816, 0x3848, 0, -2630 } }, { 0, 0, 0, 0 }, { .words = { 2816, 0x3848, 0, -1440 } }, { 0, 0, 0, 0 }, 0, 0, 0, 3, 0, 0 },
    { { .words = { 1024, 294, -5, -3482 } }, { 0, 0, 0, 0 }, { .words = { 1024, 294, -5, -4466 } }, { 0, 0, 0, 0 }, 0x52010002, 0x52010001, 0, 4, 0, 488 },
    { { .words = { 2048, 3039, 0, -433 } }, { 0, 0, 0, 0 }, { .words = { 2048, 4316, 0, -535 } }, { 0, 0, 0, 0 }, 0x52010004, 0x52010003, 0x5201000F, 6, 0, 489 },
};

SVECTOR D_dryfield_gas_station_80183214[32] = {
    { 0, -4096, 0, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { -3003, 0, 2786, 0 },
    { -2786, 0, -3003, 0 },
    { -3003, 0, 2786, 0 },
    { -3003, 0, 2786, 0 },
    { -3003, 0, 2786, 0 },
    { -277, -4076, -299, 0 },
    { 277, -4076, 299, 0 },
    { 1041, -3799, 1122, 0 },
    { -1041, -3799, -1122, 0 },
    { -3795, 0, -1541, 0 },
    { 3936, 0, -1132, 0 },
    { -3563, 0, -2021, 0 },
    { 3542, 0, -2058, 0 },
    { -2137, 0, -3494, 0 },
    { -372, 0, -4079, 0 },
    { 210, -3154, -2605, 0 },
    { 366, 0, -4080, 0 },
    { 1117, -3941, -3, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { -53, 0, 4096, 0 },
    { 25, 0, 4096, 0 },
    { -3256, 0, 2485, 0 },
    { 0, 0, -4096, 0 },
    { 0, -3958, -1055, 0 },
    { 0, -874, -4002, 0 },
    { 0, -3497, -2132, 0 },
    { -1240, 0, 3904, 0 },
    { -4084, 0, 316, 0 },
};

SVECTOR D_dryfield_gas_station_80183314[126] = {
    { 0x49ED, 0, -0x2A62, 0 },
    { -525, 0, -0x2A62, 0 },
    { 3963, 0, -4125, 0 },
    { 0x28D2, 0, -4200, 0 },
    { 3600, 0, 1750, 0 },
    { 0x49ED, 0, 1750, 0 },
    { 3663, 0, -1025, 0 },
    { -5300, 0, -0x3A98, 0 },
    { -525, 0, -4550, 0 },
    { 0x61A8, 0, 6000, 0 },
    { 0x61A8, 0, -0x3A98, 0 },
    { -5300, 0, 6000, 0 },
    { 2338, 0, 1750, 0 },
    { -525, 0, 1750, 0 },
    { 2369, 0, -1038, 0 },
    { 3513, 0, -2075, 0 },
    { -402, 0, -6369, 0 },
    { -402, -1320, -6369, 0 },
    { -402, -1320, 21, 0 },
    { -402, 0, 21, 0 },
    { 0x2710, 0, -100, 0 },
    { 0x2710, -3251, -100, 0 },
    { 0x2710, -3251, 8000, 0 },
    { 0x2710, 0, 8000, 0 },
    { 0, 0, -100, 0 },
    { 0, -3251, -100, 0 },
    { 0x32FE, 0, -4602, 0 },
    { 0x362E, -980, -3723, 0 },
    { 0x32FE, -860, -4602, 0 },
    { 0x3825, -860, -5827, 0 },
    { 0x3825, 0, -5827, 0 },
    { 0x37C6, -1222, -3283, 0 },
    { 0x3FBF, 0, -1084, 0 },
    { 0x3C8F, -980, -1963, 0 },
    { 0x3AF6, -1222, -2403, 0 },
    { 0x3FBF, -860, -1084, 0 },
    { 0x3B56, -980, -4947, 0 },
    { 0x41B6, -980, -3188, 0 },
    { 0x44E6, -860, -2308, 0 },
    { 0x401E, -1222, -3628, 0 },
    { 0x3CEE, -1222, -4507, 0 },
    { 6032, 0, -360, 0 },
    { 6032, -200, -360, 0 },
    { 9032, -200, -360, 0 },
    { 9032, 0, -360, 0 },
    { 7985, 38, 134, 0 },
    { 7985, -1162, 134, 0 },
    { 8375, -1162, -826, 0 },
    { 8375, 38, -826, 0 },
    { 0x276D, -1162, -826, 0 },
    { 0x276D, 38, -826, 0 },
    { 0x2881, -1162, 134, 0 },
    { 0x2881, 38, 134, 0 },
    { 4395, 14, -61, 0 },
    { 4395, -1034, -61, 0 },
    { 4785, -1034, -748, 0 },
    { 4785, 14, -748, 0 },
    { 8256, 14, -1248, 0 },
    { 8256, -1034, -1248, 0 },
    { 8547, -1034, -748, 0 },
    { 8547, 14, -748, 0 },
    { 5575, -1034, -748, 0 },
    { 5575, 14, -748, 0 },
    { 6066, -1034, -1048, 0 },
    { 6066, 14, -1048, 0 },
    { 2290, -350, -694, 0 },
    { -2, -1000, -92, 0 },
    { 2290, -847, -92, 0 },
    { -4, 0, -900, 0 },
    { -4, -1000, -900, 0 },
    { 2290, 0, -694, 0 },
    { 2290, 0, -92, 0 },
    { -5, 0, -5990, 0 },
    { -5, -2390, -5990, 0 },
    { -5, -2390, 686, 0 },
    { -5, 0, 686, 0 },
    { 0x4651, -2390, -1093, 0 },
    { 0x4651, 0, -1093, 0 },
    { 0x4651, 0, 3026, 0 },
    { 0x4651, -2390, 3026, 0 },
    { 0x3571, 0, -5812, 0 },
    { 0x3571, -2390, -5812, 0 },
    { 0x3571, 0, -4543, 0 },
    { 0x3571, -2390, -4543, 0 },
    { 0x3FC1, -2390, -1083, 0 },
    { 0x3FC1, 0, -1083, 0 },
    { 0x44CB, -1060, -6002, 0 },
    { 0x44CB, 0, -6002, 0 },
    { 0x44CB, 0, 3026, 0 },
    { 0x44CB, -1060, 3026, 0 },
    { -5043, 0, -6008, 0 },
    { -5043, -6000, -6008, 0 },
    { -5043, -6000, 5492, 0 },
    { -5043, 0, 5492, 0 },
    { 216, -2971, -88, 0 },
    { 216, -4558, -88, 0 },
    { 9816, -4558, -88, 0 },
    { 9816, -2971, -88, 0 },
    { 72, -3260, -79, 0 },
    { 0x2758, -3260, -79, 0 },
    { 0x2758, -3020, -979, 0 },
    { 72, -3020, -979, 0 },
    { 72, -2150, -1169, 0 },
    { 0x2758, -2150, -1169, 0 },
    { 0x2758, -2150, -79, 0 },
    { 0x3A7D, 19, 878, 0 },
    { 0x3A7D, -1051, 878, 0 },
    { 0x3A7D, -521, 9, 0 },
    { 0x3A7D, 19, 9, 0 },
    { 0x4636, -521, 9, 0 },
    { 0x4636, 19, 9, 0 },
    { 0x4636, -1051, 878, 0 },
    { 0x2736, -299, 266, 0 },
    { 0x3ABE, -299, 266, 0 },
    { 0x3ABE, -299, 46, 0 },
    { 0x2736, -299, 46, 0 },
    { 0x2736, -2, 46, 0 },
    { 0x3ABE, -2, 46, 0 },
    { 0x2736, -1869, 266, 0 },
    { 0x3ABE, -1869, 266, 0 },
    { 0x399D, 14, -2895, 0 },
    { 0x399D, -824, -2895, 0 },
    { 0x34D2, -824, -3285, 0 },
    { 0x34D2, 14, -3285, 0 },
    { 0x3478, -824, -4447, 0 },
    { 0x3478, 14, -4447, 0 },
};

GpGridFace D_dryfield_gas_station_80183704[63] = {
    { { 1, 2, 0, 3 }, 0, 2 },
    { { 5, 3, 4, 6 }, 0, 4 },
    { { 7, 8, 1, 0xFFFF }, 0, 4 },
    { { 5, 9, 0, 10 }, 0, 4 },
    { { 4, 12, 11, 13 }, 0, 4 },
    { { 5, 0, 3, 0xFFFF }, 0, 4 },
    { { 0, 10, 1, 7 }, 0, 4 },
    { { 14, 8, 13, 0xFFFF }, 0, 2 },
    { { 12, 14, 13, 0xFFFF }, 0, 2 },
    { { 15, 6, 2, 3 }, 0, 4 },
    { { 11, 9, 4, 5 }, 0, 4 },
    { { 6, 14, 4, 12 }, 0, 3 },
    { { 15, 2, 8, 1 }, 0, 4 },
    { { 7, 11, 8, 13 }, 0, 4 },
    { { 15, 14, 6, 0xFFFF }, 0, 4 },
    { { 14, 15, 8, 0xFFFF }, 0, 2 },
    { { 17, 18, 16, 19 }, 1, 0 },
    { { 21, 22, 20, 23 }, 1, 0 },
    { { 25, 21, 24, 20 }, 2, 0 },
    { { 26, 27, 28, 0xFFFF }, 3, 0 },
    { { 28, 29, 26, 30 }, 4, 0 },
    { { 32, 33, 31, 34 }, 5, 0 },
    { { 33, 32, 35, 0xFFFF }, 6, 0 },
    { { 31, 27, 32, 26 }, 7, 0 },
    { { 36, 29, 27, 28 }, 8, 0 },
    { { 33, 35, 37, 38 }, 9, 0 },
    { { 34, 33, 39, 37 }, 10, 0 },
    { { 31, 34, 40, 39 }, 0, 0 },
    { { 27, 31, 36, 40 }, 11, 0 },
    { { 42, 43, 41, 44 }, 2, 0 },
    { { 46, 47, 45, 48 }, 12, 0 },
    { { 47, 49, 48, 50 }, 2, 0 },
    { { 49, 51, 50, 52 }, 13, 0 },
    { { 51, 49, 46, 47 }, 0, 0 },
    { { 54, 55, 53, 56 }, 14, 0 },
    { { 58, 59, 57, 60 }, 15, 0 },
    { { 62, 56, 61, 55 }, 2, 0 },
    { { 64, 62, 63, 61 }, 16, 0 },
    { { 57, 64, 58, 63 }, 17, 0 },
    { { 65, 66, 67, 0xFFFF }, 18, 0 },
    { { 69, 65, 68, 70 }, 19, 0 },
    { { 65, 67, 70, 71 }, 1, 0 },
    { { 66, 65, 69, 0xFFFF }, 20, 0 },
    { { 73, 74, 72, 75 }, 21, 1 },
    { { 77, 78, 76, 79 }, 22, 1 },
    { { 72, 80, 73, 81 }, 23, 1 },
    { { 83, 81, 82, 80 }, 22, 1 },
    { { 85, 77, 84, 76 }, 24, 1 },
    { { 82, 85, 83, 84 }, 25, 1 },
    { { 87, 88, 86, 89 }, 22, 0 },
    { { 91, 92, 90, 93 }, 1, 0 },
    { { 95, 96, 94, 97 }, 26, 0 },
    { { 99, 100, 98, 101 }, 27, 0 },
    { { 101, 100, 102, 103 }, 28, 0 },
    { { 100, 99, 103, 104 }, 1, 0 },
    { { 106, 107, 105, 108 }, 22, 0 },
    { { 107, 109, 108, 110 }, 2, 0 },
    { { 111, 109, 106, 107 }, 29, 0 },
    { { 113, 114, 112, 115 }, 0, 0 },
    { { 115, 114, 116, 117 }, 2, 0 },
    { { 112, 118, 113, 119 }, 2, 0 },
    { { 121, 122, 120, 123 }, 30, 1 },
    { { 122, 124, 123, 125 }, 31, 1 },
};

s16 D_dryfield_gas_station_801839F8[6] = {
    0,
    2,
    6,
    12,
    13,
    -1,
};

s16 D_dryfield_gas_station_80183A04[8] = {
    0,
    2,
    6,
    12,
    13,
    16,
    50,
    -1,
};

s16 D_dryfield_gas_station_80183A14[10] = {
    2,
    7,
    12,
    13,
    15,
    16,
    43,
    45,
    50,
    -1,
};

s16 D_dryfield_gas_station_80183A28[15] = {
    4,
    7,
    8,
    13,
    16,
    18,
    39,
    40,
    42,
    43,
    50,
    51,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183A48[8] = {
    4,
    7,
    8,
    10,
    13,
    43,
    50,
    -1,
};

s16 D_dryfield_gas_station_80183A58[5] = {
    4,
    10,
    13,
    50,
    -1,
};

s16 D_dryfield_gas_station_80183A64[5] = {
    0,
    2,
    6,
    12,
    -1,
};

s16 D_dryfield_gas_station_80183A70[9] = {
    0,
    2,
    6,
    12,
    13,
    16,
    43,
    45,
    -1,
};

s16 D_dryfield_gas_station_80183A84[14] = {
    0,
    2,
    7,
    9,
    12,
    13,
    14,
    15,
    16,
    43,
    45,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183AA0[25] = {
    1,
    2,
    4,
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
    18,
    34,
    36,
    39,
    40,
    41,
    42,
    43,
    51,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183AD4[18] = {
    1,
    4,
    7,
    8,
    10,
    11,
    13,
    16,
    18,
    39,
    40,
    41,
    42,
    43,
    51,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183AF8[3] = {
    4,
    10,
    -1,
};

s16 D_dryfield_gas_station_80183B00[3] = {
    0,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183B08[5] = {
    0,
    6,
    12,
    45,
    -1,
};

s16 D_dryfield_gas_station_80183B14[10] = {
    0,
    1,
    9,
    12,
    14,
    15,
    45,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183B28[29] = {
    0,
    1,
    4,
    7,
    8,
    9,
    10,
    11,
    12,
    14,
    15,
    18,
    29,
    30,
    31,
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
    51,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183B64[16] = {
    1,
    4,
    8,
    10,
    11,
    18,
    29,
    34,
    36,
    37,
    39,
    41,
    51,
    52,
    53,
    -1,
};

s16 D_dryfield_gas_station_80183B84[2] = {
    10,
    -1,
};

s16 D_dryfield_gas_station_80183B88[3] = {
    0,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183B90[4] = {
    0,
    6,
    45,
    -1,
};

s16 D_dryfield_gas_station_80183B98[11] = {
    0,
    1,
    5,
    9,
    35,
    38,
    45,
    52,
    53,
    54,
    -1,
};

s16 D_dryfield_gas_station_80183BB0[25] = {
    0,
    1,
    5,
    9,
    10,
    17,
    18,
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
    51,
    52,
    53,
    54,
    58,
    59,
    60,
    -1,
};

s16 D_dryfield_gas_station_80183BE4[18] = {
    1,
    10,
    17,
    18,
    29,
    30,
    31,
    32,
    33,
    35,
    51,
    52,
    53,
    54,
    58,
    59,
    60,
    -1,
};

s16 D_dryfield_gas_station_80183C08[3] = {
    10,
    17,
    -1,
};

s16 D_dryfield_gas_station_80183C10[3] = {
    0,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183C18[8] = {
    0,
    5,
    6,
    20,
    24,
    45,
    46,
    -1,
};

s16 D_dryfield_gas_station_80183C28[18] = {
    0,
    1,
    5,
    9,
    19,
    20,
    21,
    23,
    24,
    26,
    27,
    28,
    45,
    46,
    48,
    61,
    62,
    -1,
};

s16 D_dryfield_gas_station_80183C4C[37] = {
    0,
    1,
    5,
    9,
    10,
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
    31,
    32,
    33,
    46,
    47,
    48,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    -1,
};

s16 D_dryfield_gas_station_80183C98[14] = {
    1,
    10,
    17,
    32,
    33,
    52,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    -1,
};

s16 D_dryfield_gas_station_80183CB4[3] = {
    10,
    17,
    -1,
};

s16 D_dryfield_gas_station_80183CBC[5] = {
    0,
    3,
    5,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183CC8[8] = {
    0,
    3,
    5,
    6,
    20,
    24,
    49,
    -1,
};

s16 D_dryfield_gas_station_80183CD8[21] = {
    0,
    3,
    5,
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
    45,
    46,
    47,
    48,
    49,
    61,
    62,
    -1,
};

s16 D_dryfield_gas_station_80183D04[26] = {
    1,
    3,
    5,
    10,
    19,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    44,
    47,
    48,
    49,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    -1,
};

s16 D_dryfield_gas_station_80183D38[19] = {
    1,
    3,
    5,
    10,
    21,
    22,
    23,
    25,
    44,
    47,
    48,
    49,
    55,
    56,
    57,
    58,
    59,
    60,
    -1,
};

s16 D_dryfield_gas_station_80183D60[3] = {
    10,
    49,
    -1,
};

s16 D_dryfield_gas_station_80183D68[5] = {
    0,
    3,
    5,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183D74[5] = {
    0,
    3,
    5,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183D80[5] = {
    3,
    5,
    25,
    49,
    -1,
};

s16 D_dryfield_gas_station_80183D8C[11] = {
    1,
    3,
    5,
    10,
    25,
    44,
    47,
    49,
    56,
    57,
    -1,
};

s16 D_dryfield_gas_station_80183DA4[9] = {
    1,
    3,
    5,
    10,
    44,
    49,
    56,
    57,
    -1,
};

s16 D_dryfield_gas_station_80183DB8[3] = {
    3,
    10,
    -1,
};

s16 D_dryfield_gas_station_80183DC0[3] = {
    3,
    6,
    -1,
};

s16 D_dryfield_gas_station_80183DC8[2] = {
    3,
    -1,
};

s16 D_dryfield_gas_station_80183DCC[2] = {
    3,
    -1,
};

s16 D_dryfield_gas_station_80183DD0[2] = {
    3,
    -1,
};

s16 D_dryfield_gas_station_80183DD4[3] = {
    3,
    10,
    -1,
};

s16 D_dryfield_gas_station_80183DDC[3] = {
    3,
    10,
    -1,
};

s16 * D_dryfield_gas_station_80183DE4[48] = {
    D_dryfield_gas_station_801839F8,
    D_dryfield_gas_station_80183A04,
    D_dryfield_gas_station_80183A14,
    D_dryfield_gas_station_80183A28,
    D_dryfield_gas_station_80183A48,
    D_dryfield_gas_station_80183A58,
    D_dryfield_gas_station_80183A64,
    D_dryfield_gas_station_80183A70,
    D_dryfield_gas_station_80183A84,
    D_dryfield_gas_station_80183AA0,
    D_dryfield_gas_station_80183AD4,
    D_dryfield_gas_station_80183AF8,
    D_dryfield_gas_station_80183B00,
    D_dryfield_gas_station_80183B08,
    D_dryfield_gas_station_80183B14,
    D_dryfield_gas_station_80183B28,
    D_dryfield_gas_station_80183B64,
    D_dryfield_gas_station_80183B84,
    D_dryfield_gas_station_80183B88,
    D_dryfield_gas_station_80183B90,
    D_dryfield_gas_station_80183B98,
    D_dryfield_gas_station_80183BB0,
    D_dryfield_gas_station_80183BE4,
    D_dryfield_gas_station_80183C08,
    D_dryfield_gas_station_80183C10,
    D_dryfield_gas_station_80183C18,
    D_dryfield_gas_station_80183C28,
    D_dryfield_gas_station_80183C4C,
    D_dryfield_gas_station_80183C98,
    D_dryfield_gas_station_80183CB4,
    D_dryfield_gas_station_80183CBC,
    D_dryfield_gas_station_80183CC8,
    D_dryfield_gas_station_80183CD8,
    D_dryfield_gas_station_80183D04,
    D_dryfield_gas_station_80183D38,
    D_dryfield_gas_station_80183D60,
    D_dryfield_gas_station_80183D68,
    D_dryfield_gas_station_80183D74,
    D_dryfield_gas_station_80183D80,
    D_dryfield_gas_station_80183D8C,
    D_dryfield_gas_station_80183DA4,
    D_dryfield_gas_station_80183DB8,
    D_dryfield_gas_station_80183DC0,
    D_dryfield_gas_station_80183DC8,
    D_dryfield_gas_station_80183DCC,
    D_dryfield_gas_station_80183DD0,
    D_dryfield_gas_station_80183DD4,
    D_dryfield_gas_station_80183DDC,
};

GpGridParams D_dryfield_gas_station_80183EA4[1] = {
    { NULL, D_dryfield_gas_station_80183214, D_dryfield_gas_station_80183314, D_dryfield_gas_station_80183704, D_dryfield_gas_station_80183DE4, 5300, 0x3A98, 8, 6, 4000, 63 },
};

GpViewRec D_dryfield_gas_station_80183EC8[14] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x7530, 1000 } }, 380 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 546, 0, -4059 }, { -3221, 2492, -433 }, { 2469, 3250, 332 } }, { -0x2EA0, 3781, 2877 } }, 329 },
    { { { { 3396, 0, 2288 }, { -180, 4083, 267 }, { -2281, -322, 3386 } }, { -5346, 792, 3101 } }, 230 },
    { { { { 1107, 0, -3943 }, { -2662, 3021, -747 }, { 2908, 2765, 816 } }, { -0x3206, 1776, 5127 } }, 230 },
    { { { { 3985, 0, 945 }, { 289, 3899, -1220 }, { -900, 1254, 3793 } }, { -4447, 1449, 747 } }, 230 },
    { { { { 1679, 0, -3735 }, { 590, 4044, 265 }, { 3689, -646, 1658 } }, { -0x2D82, 940, 3440 } }, 541 },
    { { { { 3799, 0, 1530 }, { 233, 4047, -580 }, { -1512, 626, 3754 } }, { -0x3815, 1503, 2328 } }, 230 },
    { { { { 2694, 0, 3085 }, { -516, 4038, 451 }, { -3041, -685, 2656 } }, { -0x29C2, 716, 3608 } }, 230 },
    { { { { 1200, 0, 3916 }, { -1855, 3607, 568 }, { -3448, -1940, 1057 } }, { -9280, 950, 3960 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
};

GpSprtCmd D_dryfield_gas_station_801840C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_gas_station_801840D0[6] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -32, 2362, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -40, 2329, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -40, 2325, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 2325, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -40, 2325, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 40, -40, 2325, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_gas_station_80184148[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_gas_station_80184160[6] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 0, 3701, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, -8, 3300, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 0, 3424, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 0, 2250, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 0, 2250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 0, 2342, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_gas_station_801841D8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 3, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_801841F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184208[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184218[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184228[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184238[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184248[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184258[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184268[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184278[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184288[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_gas_station_80184298[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_gas_station_801842A8[14] = {
    { { .empty = D_dryfield_gas_station_801840C0 }, D_dryfield_gas_station_801840C0, NULL },
    { { .elements = D_dryfield_gas_station_801840D0 }, D_dryfield_gas_station_80184148, NULL },
    { { .elements = D_dryfield_gas_station_80184160 }, D_dryfield_gas_station_801841D8, NULL },
    { { .empty = D_dryfield_gas_station_801841F8 }, D_dryfield_gas_station_801841F8, NULL },
    { { .empty = D_dryfield_gas_station_80184208 }, D_dryfield_gas_station_80184208, NULL },
    { { .empty = D_dryfield_gas_station_80184218 }, D_dryfield_gas_station_80184218, NULL },
    { { .empty = D_dryfield_gas_station_80184228 }, D_dryfield_gas_station_80184228, NULL },
    { { .empty = D_dryfield_gas_station_80184238 }, D_dryfield_gas_station_80184238, NULL },
    { { .empty = D_dryfield_gas_station_80184248 }, D_dryfield_gas_station_80184248, NULL },
    { { .empty = D_dryfield_gas_station_80184258 }, D_dryfield_gas_station_80184258, NULL },
    { { .empty = D_dryfield_gas_station_80184268 }, D_dryfield_gas_station_80184268, NULL },
    { { .empty = D_dryfield_gas_station_80184278 }, D_dryfield_gas_station_80184278, NULL },
    { { .empty = D_dryfield_gas_station_80184288 }, D_dryfield_gas_station_80184288, NULL },
    { { .empty = D_dryfield_gas_station_80184298 }, D_dryfield_gas_station_80184298, NULL },
};

GpObj4C D_dryfield_gas_station_80184350[11] = {
    { NULL, NULL, NULL, { 5791, -2544, -3505, 0 }, { { 147, -3568, 2869, 0 }, { -146, -3568, -2868, 0 }, { 147, 3568, 2869, 0 }, { -146, 3568, -2868, 0 } }, { -4101, 0, 209, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 5503, -2544, -3521, 0 }, { { -146, -3568, -2868, 0 }, { 147, -3568, 2869, 0 }, { -146, 3568, -2868, 0 }, { 147, 3568, 2869, 0 } }, { 4100, 0, -210, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 4319, -2544, -1009, 0 }, { { -210, -3568, -804, 0 }, { 211, -3568, 805, 0 }, { -210, 3568, -804, 0 }, { 211, 3568, 805, 0 } }, { 3963, 0, -1038, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 2752, -2528, -1264, 0 }, { { -1410, -3552, 524, 0 }, { 1411, -3552, -523, 0 }, { -1410, 3552, 524, 0 }, { 1411, 3552, -523, 0 } }, { -1432, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 2623, -2608, -1345, 0 }, { { 1716, -3632, -661, 0 }, { -1716, -3632, 662, 0 }, { 1716, 3632, -661, 0 }, { -1716, 3632, 662, 0 } }, { 1474, 0, 3824, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 4431, -2624, -1297, 0 }, { { 283, -3648, 1064, 0 }, { -283, -3648, -1064, 0 }, { 283, 3648, 1064, 0 }, { -283, 3648, -1064, 0 } }, { -3966, 0, 1054, 0 }, { 0, 0, 4096, 0 }, 3805, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x28FF, -2608, -1952, 0 }, { { -933, -3632, -2562, 0 }, { 911, -3632, 2537, 0 }, { -933, 3632, -2562, 0 }, { 911, 3632, 2537, 0 } }, { 3855, 0, -1395, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2A06, -2592, -1961, 0 }, { { 875, -3616, 2397, 0 }, { -875, -3616, -2396, 0 }, { 875, 3616, 2397, 0 }, { -875, 3616, -2396, 0 } }, { -3858, 0, 1407, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x361F, -2624, -2482, 0 }, { { 857, -3616, -1637, 0 }, { -856, -3616, 1638, 0 }, { 857, 3616, -1637, 0 }, { -856, 3616, 1638, 0 } }, { 3633, 0, 1900, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 5, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x29DD, -2624, -5092, 0 }, { { -867, -3616, 888, 0 }, { 867, -3616, -888, 0 }, { -867, 3616, 888, 0 }, { 867, 3616, -888, 0 } }, { -2937, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x28AD, -2688, -5043, 0 }, { { 899, -3616, -888, 0 }, { -899, -3616, 888, 0 }, { 899, 3616, -888, 0 }, { -899, 3616, 888, 0 } }, { 2895, 0, 2931, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, 129, 0 },
};

GpObj4C D_dryfield_gas_station_80184694[10] = {
    { NULL, NULL, NULL, { 2657, -108, -331, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, 0, 3, 49, 2, 0 },
    { NULL, NULL, NULL, { 192, -80, -3328, 0 }, { { 384, 0, -1440, 0 }, { 384, 0, 1024, 0 }, { -384, 0, -1440, 0 }, { -384, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1487, 0, 2, 33, 2, 0 },
    { NULL, NULL, NULL, { 0x3430, -96, -224, 0 }, { { -1776, 0, -384, 0 }, { 1776, 0, -384, 0 }, { -1776, 0, 384, 0 }, { 1776, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1814, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x36EF, -96, -4817, 0 }, { { -1877, 0, -131, 0 }, { -397, 0, -2069, 0 }, { -1650, 0, 1270, 0 }, { 694, 0, -764, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 2095, 2, 9, 0, 4, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1104, 0 }, { { -1407, 0, -624, 0 }, { 1408, 0, -624, 0 }, { -1407, 0, 624, 0 }, { 1408, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1536, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 4448, -96, 256, 0 }, { { -880, 0, -1408, 0 }, { 720, 0, -1408, 0 }, { -880, 0, -320, 0 }, { 720, 0, -320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1659, 2, 1, 255, 4, 0 },
    { NULL, NULL, NULL, { 0x36C0, -64, -3488, 0 }, { { -501, 0, -973, 0 }, { 1016, 0, 403, 0 }, { -1497, 0, -20, 0 }, { 20, 0, 1356, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1492, 2, 15, 0, 4, 0 },
    { NULL, NULL, NULL, { 0x4120, -64, 0, 0 }, { { -736, 0, -624, 0 }, { 736, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 964, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 9264, -64, -1184, 0 }, { { -911, 0, -624, 0 }, { 912, 0, -624, 0 }, { -911, 0, 624, 0 }, { 912, 0, 624, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1101, 2, 26, 0, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, -528, 0 }, { { -544, 0, -576, 0 }, { 544, 0, -576, 0 }, { -544, 0, 576, 0 }, { 544, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 791, 2, 1, 255, 130, 0 },
};

GpAreaTmdRec D_dryfield_gas_station_8018498C[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_gas_station_80184998[2] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_dryfield_gas_station_801849B0[5] = {
    { 44, 0, 0, 4226, 0, -4480, 0, 0, 0, 2, 0 },
    { 44, 0, 1, 472, -4624, -2512, 1024, 0, 0, 2, 0 },
    { 44, 0, 0, 7277, 0, -3553, 512, 0, 0, 2, 0 },
    { 44, 0, 0, 6255, 0, -5322, -1024, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_dryfield_gas_station_80184A00[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_dryfield_gas_station_80184A18[2] = {
    { 1, 0, 0, 6255, 0, -5322, -1024, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_gas_station_80184A38[12] = {
    { NULL, NULL },
    { D_map_dryfield_8017AD34, D_dryfield_gas_station_8018498C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_gas_station_801849B0, D_dryfield_gas_station_80184998 },
    { D_dryfield_gas_station_80184A18, D_dryfield_gas_station_80184A00 },
    { NULL, NULL },
};

GpLight D_dryfield_gas_station_80184A98[2] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -4000, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0x2856, 8167, 7585, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, 2000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 1966, 2048, { 0, 0 } },
};

GpRoomCoordSet D_dryfield_gas_station_80184B48[1] = {
    { 2, D_dryfield_gas_station_80184A98, 0, NULL, 0, NULL },
};

s32 D_dryfield_gas_station_80184B60[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

s32 D_dryfield_gas_station_80184B6C[3] = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

s32 D_dryfield_gas_station_80184B78[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_dryfield_gas_station_80184B84[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_gas_station_80184B8C[1] = {
    { 0, 0, 1, 0, D_dryfield_gas_station_80184B60 },
};

GpRoomParamRec D_dryfield_gas_station_80184B94[1] = {
    { 0, 0, 1, 0, D_dryfield_gas_station_80184B6C },
};

GpRoomParamRec D_dryfield_gas_station_80184B9C[1] = {
    { 0, 0, 1, 0, D_dryfield_gas_station_80184B78 },
};

GpRoomParamRec D_dryfield_gas_station_80184BA4[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec * D_dryfield_gas_station_80184BAC[8] = {
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184BA4,
    D_dryfield_gas_station_80184B94,
    D_dryfield_gas_station_80184B9C,
    D_dryfield_gas_station_80184B8C,
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184B84,
};

Task * D_dryfield_gas_station_80184BCC = NULL;

Task * D_dryfield_gas_station_80184BD0 = NULL;

Task * D_dryfield_gas_station_80184BD4 = NULL;

RoomCutsceneRec D_dryfield_gas_station_80184BD8 = { 0 };

static void func_dryfield_gas_station_801803C0(Task* task);
static void func_dryfield_gas_station_80180B4C(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
static void func_dryfield_gas_station_80181058(GpCoord* coord, SVECTOR* data, s32 arg2, s32 arg3);

/// Carries out the script command in `DgsWork::field_4`, then clears it (the
/// multi-frame commands return early until they finish). 1 places the owner at
/// the first of three 0x3E9 placements and plays two sounds; 2 and 3 hand the
/// owner a `D_dryfield_gas_station_80182E30` script record as msg 0x3F4, 2 also
/// sending msg 0x3FD and 3 placing the owner at the second placement first.
/// 4 walks the owner from the first placement to the third over 30 frames with
/// msg 0x3FE before its closing 0x3F4; 5 is `func_dryfield_gas_station_80180A60`
/// written out again; 6 spawns entry 1 of `D_dryfield_gas_station_8018312C`,
/// waits a frame and turns the display back on.
static void func_dryfield_gas_station_801803C0(Task* task)
{
    DgsWork* work;
    DgsWork* cur;
    DgsWork* eff;
    Task*    shared;
    union {
        GpAnimArg rec;
        GpMoveArg move;
    } msg;
    GpAnimArg  script;
    GpAnimArg* rec;
    u16        step;

    work = (DgsWork*)task->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_gas_station_80182E44[0], 0);
            SndEvt_EnqueueType6(0x52010011, 0, 0);
            SndEvt_EnqueueType6(0x52010012, 0, 0);
            break;
        case 2:
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.animBlock.ptr = D_dryfield_gas_station_80182E30;
                msg.rec.field_4       = 1;
                msg.rec.field_8       = 0;
                msg.rec.field_C       = 0;
                msg.rec.field_10      = 0;
                Gp_DispatchMsgPtr((Task*)cur->owner, 0x3F4, &msg.rec, 0);
            }
            Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
            break;
        case 3:
            SndEvt_EnqueueType6(0x52010013, 0, 0);
            Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_gas_station_80182E5C, 0);
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.animBlock.ptr = D_dryfield_gas_station_80182E30;
                msg.rec.field_4       = 2;
                msg.rec.field_8       = 1;
                msg.rec.field_C       = 0x1E;
                msg.rec.field_10      = 0;
                Gp_DispatchMsgPtr((Task*)cur->owner, 0x3F4, &msg.rec, 0);
            }
            break;
        case 4:
            step = work->field_6;
            switch (step) {
                case 0:
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        msg.rec.animBlock.ptr = D_dryfield_gas_station_80182E30;
                        msg.rec.field_4       = 3;
                        msg.rec.field_8       = 0;
                        msg.rec.field_C       = 0;
                        msg.rec.field_10      = 0;
                        Gp_DispatchMsgPtr((Task*)cur->owner, 0x3F4, &msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
                    Gp_DispatchMsg((Task*)work->owner, 0x3FC, 0, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    msg.move.x        = (D_dryfield_gas_station_80182E44[2].pos.vx - D_dryfield_gas_station_80182E44[0].pos.vx) / 30;
                    msg.move.y        = 0;
                    msg.move.z        = (D_dryfield_gas_station_80182E44[2].pos.vz - D_dryfield_gas_station_80182E44[0].pos.vz) / 30;
                    msg.move.field_10 = 0;
                    Gp_DispatchMsgPtr((Task*)work->owner, 0x3FE, &msg.move, 0);
                    work->field_8++;
                    if (work->field_8 < 31) {
                        return;
                    }
                    // Taken before the owner check, the record's address is in
                    // $a2 early enough that the two register-valued fields are
                    // stored through it; the constant ones still go off $sp.
                    rec = &script;
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        script.animBlock.ptr = D_dryfield_gas_station_80182E30;
                        script.field_4       = 0;
                        rec->field_8         = step;
                        rec->field_C         = 0xF;
                        script.field_10      = 0;
                        Gp_DispatchMsgPtr((Task*)cur->owner, 0x3F4, rec, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 5:
            shared = D_dryfield_gas_station_80184BD4;
            eff    = (DgsWork*)shared->work;
            if (eff->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                eff->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            Gp_DispatchMsgPtr((Task*)eff->owner, 0x3E9, &D_dryfield_gas_station_80182E74, 0);
            cur = (DgsWork*)shared->work;
            if (cur->owner != NULL) {
                msg.rec.animBlock.ptr = D_dryfield_gas_station_80182E30;
                msg.rec.field_4       = 0;
                msg.rec.field_8       = 0;
                msg.rec.field_C       = 0;
                msg.rec.field_10      = 0;
                Gp_DispatchMsgPtr((Task*)cur->owner, 0x3F4, &msg.rec, 0);
            }
            SndEvt_EnqueueType7(0x52010011, 0x3C);
            SetDispMask(1);
            break;
        case 6:
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_gas_station_8018312C, 1, 0x1E, 0);
                case 1:
                    work->field_6++;
                    return;
                case 2:
                    SetDispMask(1);
                    break;
            }
            break;
    }
    work->field_4 = 0;
}

/// Spawns the gas station's cutscene owner. State 0 refuses to run twice (a
/// `Gp_StateC08.field_A` of 1 and a live `gDisplayState.pendingMode` both mean the cutscene is already
/// up), otherwise it parks the freshly zeroed 0x10-byte `DgsWork` block in
/// `Task::work`, fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_gas_station_80184BD4` so the room's script helpers can reach that block.
/// Two kills: a failed `Mem_Malloc` kills the task outright, and state 1 kills
/// it once the session has torn down (`gGameSession->eventState`). Between the two
/// it hands slot 3 the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4 -- only when a previous state 0 already found an owner, since the
/// reloaded `work` is dereferenced unconditionally.
void func_dryfield_gas_station_801807E0(Task* task)
{
    DgsWork*  work;
    DgsWork*  work2;
    GpAnimArg script;

    switch (task->state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                work       = Mem_Malloc(0x10, false);
                task->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                     = gameGetPtrSlot(3);
                    D_dryfield_gas_station_80184BD4 = task;
                }
                work2 = (DgsWork*)task->work;
                if (work2->owner != 0) {
                    script.animBlock.ptr = D_dryfield_gas_station_80182E30;
                    script.field_4       = 0;
                    script.field_8       = 0;
                    script.field_C       = 0;
                    script.field_10      = 0;
                    Gp_DispatchMsgPtr((Task*)work2->owner, 0x3F4, &script, 0);
                }
                func_800E3FAC(0xA2, 9);
                func_800E8634(D_dryfield_gas_station_80182E8C, 0,
                              D_dryfield_gas_station_8018303C);
                task->state = task->state + 1;
                return;
            }
            return;

        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            func_dryfield_gas_station_801803C0(task);
            break;
    }
}

/// Latches the player-effect flag and kills the effects once. The 1 is loaded
/// before the branch and stored in the `jal` delay slot.
void func_dryfield_gas_station_80180944(void)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;
    if (work->playerEffActive == 0) {
        work->playerEffActive = 1;
        Gp_KillPlayerEffs();
    }
}

/// Fade task: on its first tick it allocates the 8-byte fade block and seeds
/// its three channels to 0xFF, then every frame it draws the fade overlay (the
/// red channel standing in for blue) and steps each channel down by
/// `Task::spawnArg1`, killing itself once red has gone negative.
void func_dryfield_gas_station_80180984(Task* arg0)
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
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}

/// Tells slot 3 that the cutscene is opening: it ends the weapon effect the
/// player may still be carrying (flag at `DgsWork::playerEffActive`), echoes the
/// equipped weapon back with msg 0x3E9 and, once the cutscene task has an owner,
/// hands that owner the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4. The record is a `GpAnimArg` built on the stack, only its first field
/// (the script pointer) set.
void func_dryfield_gas_station_80180A60(void)
{
    Task*     task;
    DgsWork*  work;
    DgsWork*  work2;
    GpAnimArg script;

    task = D_dryfield_gas_station_80184BD4;
    work = (DgsWork*)task->work;
    if (work->playerEffActive != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffActive = 0;
        Gp_MsgPlayerWeapon(0);
    }
    Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_gas_station_80182E74, 0);
    work2 = (DgsWork*)task->work;
    if (work2->owner != 0) {
        script.animBlock.ptr = D_dryfield_gas_station_80182E30;
        script.field_4       = 0;
        script.field_8       = 0;
        script.field_C       = 0;
        script.field_10      = 0;
        Gp_DispatchMsgPtr((Task*)work2->owner, 0x3F4, &script, 0);
    }
    SndEvt_EnqueueType7(0x52010011, 0x3C);
    SetDispMask(1);
}

/// Hands the cutscene task the script command `arg0` to carry out, starting
/// it from its first step.
void func_dryfield_gas_station_80180B2C(s16 arg0)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}

/// Draws a pulsing glow at `arg1` in `arg0`'s space. `arg1` is rotated by
/// `arg0`'s `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` with a single `RTPS` into a 0x14-byte `G_SCRATCH_HEAD` block;
/// nothing is drawn when its `otz` is 16 or less. `arg3` is a signed
/// half-extent, so the on-screen half width is `(s16)arg3 * 32 / otz`; two
/// gouraud `POLY_G4` wedges and two `LINE_G3` diagonals cross the projected
/// centre, whose green and blue pulse as `rsin(animFrame * arg2) / 34 + 0x78`.
static void func_dryfield_gas_station_80180B4C(GpCoord* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               i;
    s32               color;
    s32               pulse;
    s32               twice;
    s32               t;
    s32               t2;

    Gp_UpdateCoord(arg0);
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
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse            = rsin(gDisplayState.animFrame * (s16)arg2);
        i                = 0;
        block->halfWidth = ((s16)arg3 << 5) / ((RoomShaftScratch*)(head - 0x14))->otz;
        color            = pulse / 34 + 0x78;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + (u16)block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            twice                          = i << 1;
            prim->y1                       = (block->sy - (u16)block->halfWidth) + block->halfWidth * twice;
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
            setRGB1(line, 0, color, color);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Draws a pulsing cyan glow at `data` in `coord`'s space: the point is
/// projected through `GsWSMATRIX`, and nothing is drawn when its `otz` is 16 or
/// less. Around the projected centre it lays a fan of gouraud `POLY_G4`
/// wedges of radius `rOuter`, each paired with a brighter one of half that
/// radius, then four quads reaching out from `rInner` towards `rOuter`.
/// The centre vertex's intensity is `rsin(animFrame * arg2) / 34 + 0x78`,
/// halved on the outer wedges and on the four quads.
static void func_dryfield_gas_station_80181058(GpCoord* coord, SVECTOR* data, s32 arg2, s32 arg3)
{
    u8*              head;
    RoomGlowScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;

    Gp_UpdateCoord(coord);
    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (RoomGlowScratch*)tmp;
    }

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0();
    gte_stsv(&((RoomGlowScratch*)(head - 0x18))->vec);
    block->vec.vx += coord->workm.t[0];
    block->vec.vy += coord->workm.t[1];
    block->vec.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&block->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz > 16) {
        pulse         = rsin(gDisplayState.animFrame * (s16)arg2);
        ang           = 0;
        size          = (s16)arg3;
        block->rOuter = (size * 64) / ((RoomGlowScratch*)(head - 0x18))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomGlowScratch*)(head - 0x18))->otz;
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
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame effect: draws the gas station's shaft with the task's own model
/// coordinate, then advances the room's `Gp_State1C`. `Task::extra` is the
/// task's `TmdObject`, so `field_8` is the coordinate both draws share. The
/// stage-visit byte `gGameSession->at4.loc.view` is used as a bit index: bits 4, 6,
/// 11 and 12 (`0x1850`) select `func_dryfield_gas_station_80180B4C` with the wide half-extent 0x80,
/// and any other non-zero bit selects `func_dryfield_gas_station_80181058`
/// with 0x40.
void func_dryfield_gas_station_80181A78(Task* arg0)
{
    s32      mask;
    GpCoord* coord;

    mask  = 1 << gGameSession->at4.loc.view;
    coord = arg0->extra.tmd->coords;
    if (mask & 0x1850) {
        func_dryfield_gas_station_80180B4C(coord, &D_dryfield_gas_station_80183144, 0x60, 0x80);
    } else if (mask != 0) {
        func_dryfield_gas_station_80181058(coord, &D_dryfield_gas_station_80183144, 0x60, 0x40);
    }
    Gp_State1C->roomEffectMode = 2;
}
