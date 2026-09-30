#include "dryfield_night_factory_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "rooms/dryfield_night_factory.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_factory.h"

#include "rooms/room_common.h"

/// The two argument blocks one of the turn handlers hands `Gp_SpawnScript18`,
/// one pair per stage variant.

static void func_dryfield_night_factory_8017D6F8(Task* task);
static s32  func_dryfield_night_factory_8017F00C(Task* task);
static s32  func_dryfield_night_factory_8017F1DC(Task* task);
static void func_dryfield_night_factory_8017FBF4(Task* task);
static void func_dryfield_night_factory_8017FD5C(Task* task);
static s32  func_dryfield_night_factory_8017FDC8(Task* task);

static void func_dryfield_night_factory_8017D858(Task* task, s32 remapFaces, s32 useAltTemplate);
static void func_dryfield_night_factory_8017FA08(Task* task);
static void func_dryfield_night_factory_8017FB48(Task* task);
static void func_dryfield_night_factory_8017FB68(Task* task);
static void func_dryfield_night_factory_8017FBC8(Task* arg0);

/// State handlers of the factory model task: set-up, the per-frame state and
/// `taskKill`.
static const TaskFuncTable3 D_dryfield_night_factory_8017D5C4 = {
    { func_dryfield_night_factory_8017D6F8, func_dryfield_night_factory_8017FA08, taskKill },
};

/// State handlers of the cutscene task: set-up, the cutscene sequence and
/// `taskKill`.
static const TaskFuncTable3 D_dryfield_night_factory_8017D5D0 = {
    { func_dryfield_night_factory_8017FBF4, func_dryfield_night_factory_8017FD5C, taskKill },
};

/// The cutscene sequence's handler table: the flag watcher of state 0 and the
/// two movements it arms.
static const NightFactoryCutsceneTable3 D_dryfield_night_factory_8017D5DC = {
    { func_dryfield_night_factory_8017FDC8, func_dryfield_night_factory_8017F00C, func_dryfield_night_factory_8017F1DC },
};

SpriteBatch D_dryfield_night_factory_80189A14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_factory_80189A24[19] = {
    { { .empty = D_dryfield_night_factory_80187EC0 }, D_dryfield_night_factory_80187EC0, NULL },
    { { .elements = D_dryfield_night_factory_80187ED0 }, D_dryfield_night_factory_80187FD4, NULL },
    { { .elements = D_dryfield_night_factory_80188004 }, D_dryfield_night_factory_801880F4, NULL },
    { { .elements = D_dryfield_night_factory_8018811C }, D_dryfield_night_factory_80188784, NULL },
    { { .empty = D_dryfield_night_factory_801887BC }, D_dryfield_night_factory_801887BC, NULL },
    { { .elements = D_dryfield_night_factory_801887CC }, D_dryfield_night_factory_80188A4C, NULL },
    { { .elements = D_dryfield_night_factory_80188A84 }, D_dryfield_night_factory_80188D90, NULL },
    { { .elements = D_dryfield_night_factory_80188DD0 }, D_dryfield_night_factory_80188EC0, NULL },
    { { .elements = D_dryfield_night_factory_80188ED8 }, D_dryfield_night_factory_80188F28, NULL },
    { { .empty = D_dryfield_night_factory_80188F40 }, D_dryfield_night_factory_80188F40, NULL },
    { { .empty = D_dryfield_night_factory_80188F50 }, D_dryfield_night_factory_80188F50, NULL },
    { { .empty = D_dryfield_night_factory_80188F60 }, D_dryfield_night_factory_80188F60, NULL },
    { { .empty = D_dryfield_night_factory_80188F70 }, D_dryfield_night_factory_80188F70, NULL },
    { { .empty = D_dryfield_night_factory_80188F80 }, D_dryfield_night_factory_80188F80, NULL },
    { { .elements = D_dryfield_night_factory_80188F90 }, D_dryfield_night_factory_801895A8, NULL },
    { { .elements = D_dryfield_night_factory_801895D8 }, D_dryfield_night_factory_80189858, NULL },
    { { .empty = D_dryfield_night_factory_80189880 }, D_dryfield_night_factory_80189880, NULL },
    { { .elements = D_dryfield_night_factory_80189890 }, D_dryfield_night_factory_801899E4, NULL },
    { { .elements = D_dryfield_night_factory_80189890 }, D_dryfield_night_factory_801899E4, NULL },
};

GpPointLight D_dryfield_night_factory_80189B08[4] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3051, -1864, 8762 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1419, 4022 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5657, -1403, 9444 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1759, 1784, 1784, { 0, 0 } }, 1000, 1319 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3051, -1864, 2398 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1702, 3699 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3051, -1864, 5767 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1522, 4180 },
};

GpRoomCoordSet D_dryfield_night_factory_80189C88[1] = {
    { 0, NULL, 4, D_dryfield_night_factory_80189B08, 0, NULL },
};

GpObj4C D_dryfield_night_factory_80189CA0[14] = {
    { NULL, NULL, NULL, { 2925, -2368, 1935, 0 }, { { -144, -2976, -2288, 0 }, { -144, 2976, -2288, 0 }, { 144, -2976, 2288, 0 }, { 144, 2976, 2288, 0 } }, { -4092, 0, 257, 0 }, { 0, 0, 4096, 0 }, 3753, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2704, -2368, 1920, 0 }, { { 144, -2976, 2400, 0 }, { 144, 2976, 2400, 0 }, { -144, -2976, -2400, 0 }, { -144, 2976, -2400, 0 } }, { 4090, 0, -246, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 1312, -2304, 5840, 0 }, { { 1616, -2976, 160, 0 }, { 1616, 2976, 160, 0 }, { -1616, -2976, -160, 0 }, { -1616, 2976, -160, 0 } }, { 405, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3386, 0, 4, 7, 1, 0 },
    { NULL, NULL, NULL, { 1280, -2368, 5712, 0 }, { { -1632, -2976, -144, 0 }, { -1632, 2976, -144, 0 }, { 1632, -2976, 144, 0 }, { 1632, 2976, 144, 0 } }, { -362, 0, 4091, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 1712, -2336, 8096, 0 }, { { -1520, -2976, 1280, 0 }, { -1520, 2976, 1280, 0 }, { 1520, -2976, -1280, 0 }, { 1520, 2976, -1280, 0 } }, { 2640, 0, 3134, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 1760, -2336, 8289, 0 }, { { 1520, -2976, -1280, 0 }, { 1520, 2976, -1280, 0 }, { -1520, -2976, 1280, 0 }, { -1520, 2976, 1280, 0 } }, { -2641, 0, -3136, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 3232, -2336, 5040, 0 }, { { 64, -2976, -752, 0 }, { 64, 2976, -752, 0 }, { -64, -2976, 752, 0 }, { -64, 2976, 752, 0 } }, { -4094, 0, -349, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 3088, -2368, 5087, 0 }, { { -64, -2976, 752, 0 }, { -64, 2976, 752, 0 }, { 64, -2976, -752, 0 }, { 64, 2976, -752, 0 } }, { 4091, 0, 348, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 3360, -2336, 9648, 0 }, { { 112, -2976, -2032, 0 }, { 112, 2976, -2032, 0 }, { -112, -2976, 2032, 0 }, { -112, 2976, 2032, 0 } }, { -4097, 0, -227, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 3184, -2272, 9648, 0 }, { { -128, -2976, 2112, 0 }, { -128, 2976, 2112, 0 }, { 128, -2976, -2112, 0 }, { 128, 2976, -2112, 0 } }, { 4091, 0, 247, 0 }, { 0, 0, 4096, 0 }, 3647, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 1104, -2144, 4080, 0 }, { { -1152, -2976, 16, 0 }, { -1152, 2976, 16, 0 }, { 1152, -2976, -16, 0 }, { 1152, 2976, -16, 0 } }, { 56, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 1040, -2144, 4192, 0 }, { { 1360, -2976, -48, 0 }, { 1360, 2976, -48, 0 }, { -1360, -2976, 48, 0 }, { -1360, 2976, 48, 0 } }, { -146, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 4992, -2080, 4275, 0 }, { { 1360, -2976, 0, 0 }, { 1360, 2976, 0, 0 }, { -1360, -2976, 0, 0 }, { -1360, 2976, 0, 0 } }, { 0, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 2, 6, 1, 0 },
    { NULL, NULL, NULL, { 5008, -2112, 4160, 0 }, { { -1600, -2976, 0, 0 }, { -1600, 2976, 0, 0 }, { 1600, -2976, 0, 0 }, { 1600, 2976, 0, 0 } }, { 0, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 6, 2, 129, 0 },
};

GpRoomBoundVec D_dryfield_night_factory_8018A0C8[20] = {
    { 19, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 763, 757, 658, 746 },
    { 870, 852, 838, 857 },
    { 638, 625, 617, 628 },
    { 16, 16, 16, 16 },
    { 458, 450, 448, 452 },
    { 534, 526, 518, 528 },
    { 519, 513, 500, 513 },
    { 479, 477, 477, 477 },
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

GpObj4C D_dryfield_night_factory_8018A168[19] = {
    { NULL, NULL, NULL, { 5328, -48, 1296, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, 0, 25, 17, 2, 0 },
    { NULL, NULL, NULL, { 5712, -64, 0x2B70, 0 }, { { -672, 0, -928, 0 }, { 672, 0, -928, 0 }, { -672, 0, 928, 0 }, { 672, 0, 928, 0 } }, { 0, 4108, 0, 0 }, { -3784, 0, -1567, 0 }, 1144, 2, 5, 255, 2, 0 },
    { NULL, NULL, NULL, { 336, -64, 544, 0 }, { { -688, 0, -672, 0 }, { 688, 0, -672, 0 }, { -688, 0, 672, 0 }, { 688, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 960, 2, 6, 255, 2, 0 },
    { NULL, NULL, NULL, { 5856, -64, 5600, 0 }, { { -496, 0, -576, 0 }, { 496, 0, -576, 0 }, { -496, 0, 576, 0 }, { 496, 0, 576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 759, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { 6224, -64, 3968, 0 }, { { -1264, 0, -416, 0 }, { 1264, 0, -416, 0 }, { -1264, 0, 416, 0 }, { 1264, 0, 416, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1330, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 6240, -64, 4816, 0 }, { { -1264, 0, -448, 0 }, { 1264, 0, -448, 0 }, { -1264, 0, 448, 0 }, { 1264, 0, 448, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, 4096, 0 }, 1336, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 384, -64, 7328, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, 0, 22, 34, 2, 0 },
    { NULL, NULL, NULL, { 5440, -64, 7088, 0 }, { { -528, 0, -992, 0 }, { 528, 0, -992, 0 }, { -528, 0, 992, 0 }, { 528, 0, 992, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1123, 0, 24, 49, 2, 0 },
    { NULL, NULL, NULL, { 1568, -64, 7104, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, 2, 12, 255, 2, 0 },
    { NULL, NULL, NULL, { 3712, -64, 2528, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 3424, -64, 0, 0 }, { { -1872, 0, -304, 0 }, { 464, 0, -304, 0 }, { -1072, 0, 1296, 0 }, { 464, 0, 1296, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1894, 2, 16, 0, 4, 0 },
    { NULL, NULL, NULL, { 4816, -64, 928, 0 }, { { -768, 0, -368, 0 }, { 768, 0, -368, 0 }, { -768, 0, 368, 0 }, { 768, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 851, 2, 17, 0, 2, 0 },
    { NULL, NULL, NULL, { 320, -64, 2336, 0 }, { { -512, 0, -1488, 0 }, { 1408, 0, -912, 0 }, { -512, 0, 880, 0 }, { 1408, 0, 880, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1673, 2, 18, 0, 4, 0 },
    { NULL, NULL, NULL, { 1799, -64, 0x2787, 0 }, { { -775, 0, 52, 0 }, { -269, 0, -621, 0 }, { 322, 0, 592, 0 }, { 724, 0, -20, 0 } }, { 0, 4099, 0, 0 }, { 3406, 0, -2276, 0 }, 775, 2, 20, 0, 2, 0 },
    { NULL, NULL, NULL, { 3456, -64, 7440, 0 }, { { -2208, 0, -2688, 0 }, { 1760, 0, -2688, 0 }, { -2208, 0, 2688, 0 }, { 1760, 0, 2688, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 3472, 2, 24, 0, 4, 0 },
    { NULL, NULL, NULL, { 1248, -64, 8960, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, 2, 22, 0, 2, 0 },
    { NULL, NULL, NULL, { 4816, -64, 1744, 0 }, { { -928, 0, -384, 0 }, { 928, 0, -384, 0 }, { -928, 0, 384, 0 }, { 928, 0, 384, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, -4091, 0 }, 1003, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 4751, -64, 3455, 0 }, { { -947, 0, -557, 0 }, { 1098, 0, -195, 0 }, { -1097, 0, 196, 0 }, { 948, 0, 558, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, 4091, 0 }, 1108, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { 5632, -64, 9808, 0 }, { { -672, 0, -368, 0 }, { 672, 0, -368, 0 }, { -672, 0, 368, 0 }, { 672, 0, 368, 0 } }, { 0, 4102, 0, 0 }, { -3612, 0, 1931, 0 }, 765, 2, 21, 0, 130, 0 },
};

GpAreaVariant D_dryfield_night_factory_8018A70C[11] = { 0 };

s32 D_dryfield_night_factory_8018A764[3] = {
    0x10000051,
    0x10000053,
    0x10000051,
};

s32 D_dryfield_night_factory_8018A770[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_dryfield_night_factory_8018A77C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_factory_8018A784[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_night_factory_8018A78C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_factory_8018A764 },
};

GpRoomParamRec D_dryfield_night_factory_8018A794[1] = {
    { 0, 0, 1, 0, D_dryfield_night_factory_8018A770 },
};

GpRoomParamRec* D_dryfield_night_factory_8018A79C[8] = {
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A784,
    D_dryfield_night_factory_8018A78C,
    D_dryfield_night_factory_8018A794,
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A77C,
};

GpScriptCmd D_dryfield_night_factory_8018A7BC[3] = { { 0x201, 1 }, { 0, 0x101 }, { 0, 0 } };

GpScriptRec D_dryfield_night_factory_8018A7C8[3] = { { 255, 255, 8, 1 }, { 150, 80, 20, 1 }, { 0, 0, 5, 0 } };

RoomEventMsg D_dryfield_night_factory_8018A7D4 = { 0 };

u8 D_dryfield_night_factory_8018A7DC = 0;

TaskDesc* D_dryfield_night_factory_8018A7E0 = NULL;

TaskDesc* D_dryfield_night_factory_8018A7E4 = NULL;

Task** D_dryfield_night_factory_8018A7E8 = NULL;

RoomEventReq D_dryfield_night_factory_8018A7EC = { 0 };

static s32 func_dryfield_night_factory_8017DA54(Task* task);
static s32 func_dryfield_night_factory_8017DDD4(Task* task);
static s32 func_dryfield_night_factory_8017E13C(Task* task);
static s32 func_dryfield_night_factory_8017E480(Task* task);
static s32 func_dryfield_night_factory_8017E7A4(Task* task);
static s32 func_dryfield_night_factory_8017EBD4(Task* task);

/// State 0 of the room's factory model: allocate the work block, seed it from
/// the progress nibble, point the model's coordinate at the seeded position
/// and the light/color matrices at the block's own, then pick the spawn table
/// for this session variant and hand the model to its own state machine.
///
/// The two spawn tables are passed straight to `Task_SpawnFromTable` from each
/// arm rather than through a variable: the argument is then a bare symbol, so
/// the `lui`/`addiu` pair is built in `$a0` itself and `jump2` merges the two
/// arms' identical tails back into one call.
static void func_dryfield_night_factory_8017D6F8(Task* task)
{
    NightFactoryWork* work;
    GfxCoord*         coord;
    TmdObject*        obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work     = (TaskIdMap*)work;
    work->field_0  = GameFlag_GetNibble(0x49);
    work->field_16 = -1;
    work->field_17 = -1;
    obj->flags    &= (u16)~TMD_OBJECT_HIDDEN;
    if (work->field_0 & 1) {
        work->field_10.value = 0x4000000;
        RotMatrixY(0x4000000, &coord->coord);
    }
    if (work->field_0 & 2) {
        work->field_C.value = 0xFDC60000;
    } else {
        work->field_C.value = 0;
    }
    coord->coord.t[0] = 0xE4C;
    coord->coord.t[1] = work->field_C.part.whole;
    coord->coord.t[2] = 0x1AAE;
    func_dryfield_night_factory_8017FB68(task);
    func_dryfield_night_factory_8017D858(task, 1, 0);
    if (gGameSession->at4.loc.stage == 2) {
        Task_SpawnFromTable(D_dryfield_factory_80186E28, 7, 0, task);
    } else {
        Task_SpawnFromTable(D_dryfield_night_factory_80186DE0, 7, 0, task);
    }
    task->exitCallback  = func_dryfield_night_factory_8017FB48;
    task->killCountdown = 0;
    task->state++;
}

/// Rebuilds four faces of the stage variant's collision grid from a template
/// moved into the frame of the task's model: the template normals are rotated
/// into grid normals 2..5 and its corners rotated and translated into corners
/// 8..15. `useAltTemplate` picks the second template, and `remapFaces` also
/// copies the template's four face records into faces 2..5, rebased onto those
/// slots. The model's set-up state remaps; the per-frame state passes bit 0 of
/// game flag 0x49 as `useAltTemplate`.
static void func_dryfield_night_factory_8017D858(Task* task, s32 remapFaces, s32 useAltTemplate)
{
    long          flag;
    GfxCoord*     coord;
    MATRIX*       m;
    GpGridParams* geom;
    GpGridParams* src;
    SVECTOR*      s;
    SVECTOR*      d;
    GpGridFace*   sf;
    GpGridFace*   df;
    u16*          sv;
    u16*          dv;
    s32           i;
    s32           j;

    coord = task->extra.tmd->coords;
    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_night_factory_80187BF0;
    }
    if (useAltTemplate != 0) {
        src = &D_dryfield_night_factory_80186DBC;
    } else {
        src = &D_dryfield_night_factory_80186CF0;
    }

    m = &coord->coord;
    s = src->field_4;
    d = geom->field_4 + 2;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    s = src->field_8;
    d = geom->field_8 + 8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }

    if (remapFaces != 0) {
        sf = src->field_C;
        df = geom->field_C + 2;
        for (i = 0; i < 4; i++) {
            j  = 0;
            dv = df->verts;
            sv = sf->verts;
            do {
                *dv++ = *sv++ + 8;
            } while (++j < 4);
            df->normalIndex  = sf->normalIndex + 2;
            df->surfaceClass = sf->surfaceClass;
            df++;
            sf++;
        }
    }
}

static s32 func_dryfield_night_factory_8017DA54(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    s32               done  = 0;
    OverlayMat*       mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += 0x18000;
            if (work->field_8 > 0x40000) {
                work->field_8 = 0x40000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value > 0x4000000) {
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += -0x8000;
            if (work->field_8 < -0x20000) {
                work->field_8 = -0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value <= 0x4000000) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_10.value = 0x4000000;
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                 = 1;
        work->field_10.value = 0x4000000;
        work->field_16       = 4;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixY(work->field_10.part.whole, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}

/// The handler that follows `func_dryfield_night_factory_8017E13C` when bit 0 of
/// game flag 0x49 is clear.
static s32 func_dryfield_night_factory_8017DDD4(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    s32               done  = 0;
    OverlayMat*       mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += -0x18000;
            if (work->field_8 < -0x40000) {
                work->field_8 = -0x40000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value < 0) {
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += 0x8000;
            if (work->field_8 > 0x20000) {
                work->field_8 = 0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value >= 0) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_10.value = 0;
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                 = 1;
        work->field_10.value = 0;
        work->field_16       = 4;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixY(work->field_10.part.whole, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}

/// The handler the model runs while bit 1 of game flag 0x49 is set, and -- when
/// bit 0 is set with it -- the handler that follows.
static s32 func_dryfield_night_factory_8017E13C(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    s32               done  = 0;

    switch (work->field_17) {
        case 0:
            work->field_4 = 0;
            work->field_17++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x52170008, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x53170008, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_17++;
            break;
        case 2:
            work->field_4 += -0xC000;
            if (work->field_4 < -0x30000) {
                work->field_4 = -0x30000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value < -0x23A0000) {
                work->field_17++;
            }
            break;
        case 3:
            work->field_4 += 0xC000;
            if (work->field_4 > 0xC000) {
                work->field_4 = 0xC000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value >= -0x23A0000) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x52170008, 1);
                    Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x53170008, 1);
                    Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_17++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_17 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x52170008, 1);
            Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x53170008, 1);
            Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                = 1;
        work->field_C.value = -0x23A0000;
        work->field_17      = 4;
    }
    coord->coord.t[1]   = work->field_C.part.whole;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}

/// The handler the model runs while bit 1 of game flag 0x49 is clear, and --
/// when bit 0 is set with it -- the handler that follows.
static s32 func_dryfield_night_factory_8017E480(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    s32               done  = 0;

    switch (work->field_17) {
        case 0:
            work->field_4 = 0;
            work->field_17++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x52170008, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x53170008, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_17++;
            break;
        case 2:
            work->field_4 += 0xC000;
            if (work->field_4 > 0x30000) {
                work->field_4 = 0x30000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value > 0) {
                work->field_17++;
            }
            break;
        case 3:
            work->field_4 += -0xC000;
            if (work->field_4 < -0xC000) {
                work->field_4 = -0xC000;
            }
            work->field_C.value += work->field_4;
            if (work->field_C.value <= 0) {
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x52170008, 1);
                    Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x53170008, 1);
                    Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_17++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_17 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x52170008, 1);
            Gp_EnqueueStageSnd6(0x52170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x53170008, 1);
            Gp_EnqueueStageSnd6(0x53170010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        done                = 1;
        work->field_C.value = 0;
        work->field_17      = 4;
    }
    coord->coord.t[1]   = work->field_C.part.whole;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}

static s32 func_dryfield_night_factory_8017E7A4(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    s32               done  = 0;
    OverlayMat*       mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += 0x18000;
            if (work->field_8 > 0x40000) {
                work->field_8 = 0x40000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value > 0x800000) {
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x52170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18(D_dryfield_factory_8018A39C, D_dryfield_factory_8018A3A8);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18(D_dryfield_night_factory_8018A7BC, D_dryfield_night_factory_8018A7C8);
                }
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += -0xC000;
            if (work->field_8 < -0x20000) {
                work->field_8 = -0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value <= 0) {
                work->field_0 = GameFlag_GetNibble(0x49) & 0xFE;
                GameFlag_SetNibble(0x49, work->field_0);
                work->field_10.value = 0;
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        work->field_0 = GameFlag_GetNibble(0x49) & 0xFE;
        GameFlag_SetNibble(0x49, work->field_0);
        work->field_10.value = 0;
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_16 = 4;
        done           = 1;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixY(work->field_10.part.whole, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}

/// The handler that follows `func_dryfield_night_factory_8017E480` when bit 0 of
/// game flag 0x49 is clear.
static s32 func_dryfield_night_factory_8017EBD4(Task* task)
{
    NightFactoryWork* work  = (NightFactoryWork*)task->work;
    GfxCoord*         coord = task->extra.tmd->coords;
    s32               done  = 0;
    OverlayMat*       mat;

    switch (work->field_16) {
        case 0:
            work->field_8 = 0;
            work->field_16++;
            break;
        case 1:
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000F, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            work->field_16++;
            break;
        case 2:
            work->field_8 += -0x18000;
            if (work->field_8 < -0x40000) {
                work->field_8 = -0x40000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value < 0x3800000) {
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x52170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18(D_dryfield_factory_8018A39C, D_dryfield_factory_8018A3A8);
                } else {
                    Gp_EnqueueStageSnd6(0x53170012, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    Gp_SpawnScript18(D_dryfield_night_factory_8018A7BC, D_dryfield_night_factory_8018A7C8);
                }
                work->field_16++;
            }
            break;
        case 3:
            work->field_8 += 0xC000;
            if (work->field_8 > 0x20000) {
                work->field_8 = 0x20000;
            }
            work->field_10.value += work->field_8;
            if (work->field_10.value >= 0x4000000) {
                work->field_0 = GameFlag_GetNibble(0x49) | 1;
                GameFlag_SetNibble(0x49, work->field_0);
                work->field_10.value = 0x4000000;
                func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd7(0x5217000F, 1);
                    Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(0x5317000F, 1);
                    Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                work->field_16++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->field_16 - 1) < 3 && Pad_CheckButtons(0, 1, 0x800) != 0 && (s16)work->field_14 >= 0xB) {
        work->field_0 = GameFlag_GetNibble(0x49) | 1;
        GameFlag_SetNibble(0x49, work->field_0);
        work->field_10.value = 0x4000000;
        func_dryfield_night_factory_8017FBC8(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->at4.loc.stage == 2) {
            Gp_EnqueueStageSnd7(0x5217000F, 1);
            Gp_EnqueueStageSnd6(0x52170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(0x5317000F, 1);
            Gp_EnqueueStageSnd6(0x53170011, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_16 = 4;
        done           = 1;
    }
    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixY(work->field_10.part.whole, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}

/// Cutscene state 1: plays the movement's sound, swings the model about X
/// towards -0x300, overshooting and settling back on it, and answers non-zero
/// once it has settled.
static s32 func_dryfield_night_factory_8017F00C(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GfxCoord*                 coord = task->extra.tmd->coords;
    OverlayMat*               mat;
    s32                       ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            if (gGameSession->at4.loc.stage == 2) {
                Gp_EnqueueStageSnd6(0x5217000D, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(0x5317000D, (s8)Gp_GetObjPan(coord),
                                    (s8)gpGetObjDepth(coord));
            }
            work->step++;
            break;
        case 1:
            work->field_0 += -0x28000;
            if (work->field_0 < -0x300000) {
                work->field_0 = -0x300000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value < -0x3000000) {
                work->step++;
            }
            break;
        case 2:
            work->field_0 += 0x40000;
            if (work->field_0 > 0x100000) {
                work->field_0 = 0x100000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value >= -0x3000000) {
                work->field_4.value = -0x3000000;
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixX(work->field_4.part.whole, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return ret;
}

/// Cutscene state 2: swings the model about X back up past 0, playing the
/// movement's sound as it gets there, and answers non-zero afterwards.
static s32 func_dryfield_night_factory_8017F1DC(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    GfxCoord*                 coord = task->extra.tmd->coords;
    OverlayMat*               mat;
    s32                       ret = 0;

    switch (work->step) {
        case 0:
            work->field_0 = 0;
            work->step++;
            break;
        case 1:
            work->field_0 += 0x20000;
            if (work->field_0 > 0x700000) {
                work->field_0 = 0x700000;
            }
            work->field_4.value += work->field_0;
            if (work->field_4.value > 0) {
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x5217000E, (s8)Gp_GetObjPan(coord),
                                        (s8)gpGetObjDepth(coord));
                } else {
                    Gp_EnqueueStageSnd6(0x5317000E, (s8)Gp_GetObjPan(coord),
                                        (s8)gpGetObjDepth(coord));
                }
                work->step++;
            }
            break;
        default:
            ret = 1;
            break;
    }

    mat                = (OverlayMat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    RotMatrixX(work->field_4.part.whole, &mat->mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return ret;
}

/// Cutscene driver for the factory room: silences both weapons, runs the cap
/// (cutscene) command in `Task::spawnArg1`, then waits for the cap to report
/// event key 3 before setting the two progress flags and starting the follow-up
/// cap slot. Any state past 4 restores the weapons and kills the task.
void func_dryfield_night_factory_8017F330(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x48) <= 0) {
                if (gGameSession->at4.loc.stage == 2) {
                    func_dryfield_factory_80181B38(0);
                } else {
                    func_dryfield_night_factory_80181B38(0);
                }
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        case 3:
            if (Gp_GetCapEventKey() == 3) {
                GameFlag_SetNibble(0x48, 1);
                GameFlag_SetNibble(0x4A, 1);
                if (gGameSession->at4.loc.stage == 2) {
                    func_dryfield_factory_80181B38(1);
                    func_dryfield_factory_80181620(1);
                } else {
                    func_dryfield_night_factory_80181B38(1);
                    func_dryfield_night_factory_80181620(1);
                }
                Gp_StartCapSlot(task->spawnArg1.value, 1, 2);
            }
        advance:
            task->state++;
            return;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            /* fallthrough */
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
            taskKill(task);
            break;
    }
}

/// The room's second cutscene: states 0..2 silence both weapons, run the cap in
/// `Task::spawnArg1` and wait for it to report event key 1; states 3 and 6 count
/// `Task::killCountdown` up to and back down from 0x1E and tint the screen with
/// the count scaled to 0xFF over 30 steps; states 4 and 5 publish the progress
/// flags and tint it white, and anything past 6 restores the weapons and kills
/// the task.
///
/// `fade` does two jobs on purpose: state 4 reads the session variant through
/// it before testing it. That cross-block use is what makes the state-3/6 tint
/// value a *global* pseudo, and `local-alloc` only folds the `(u8)fade`
/// conversion into the division's quantity when that pseudo is local to one
/// block -- global, the conversion keeps its own quantity and takes `$a0` from
/// the argument move, while the division chain keeps `$v1`.
void func_dryfield_night_factory_8017F4F4(Task* task)
{
    u8 fade;

    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x47) != 0) {
                goto kill;
            }
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd1(task->spawnArg1.value);
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                task->killCountdown = 0;
                if (gGameSession->at4.loc.stage == 2) {
                    Gp_EnqueueStageSnd6(0x5217000C, 0, 0);
                }
                goto advance;
            }
            task->state = -1;
            return;
        case 3:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown < 0x1E) {
                goto draw;
            }
            goto bump;
        case 4:
            gGameSession->viewDirty = 1;
            GameFlag_SetNibble(0x47, 1);
            fade = gGameSession->at4.loc.stage;
            if (fade == 2) {
                Gp_EnqueueStageSnd6(0x5217000B, 0, 0);
            }
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            goto advance;
        case 5:
            Mc_SaveData[0].state.at4.loc.room = 2;
            gGameSession->at4.loc.room        = 2;
            gGameSession->roomObjsDirty       = 1;
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            goto advance;
        case 1:
        advance:
            task->state = task->state + 1;
            return;
        case 6:
            task->killCountdown = task->killCountdown - 1;
            if (task->killCountdown > 0) {
                goto draw;
            }
        bump:
            task->state = task->state + 1;
        draw:
            fade = (task->killCountdown * 255) / 30;
            Fade_DrawOverlay(fade, fade, fade, 2);
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
        kill:
            taskKill(task);
            return;
    }
}

/// Restores two faces of the stage variant's collision grid -- normals,
/// corners and face records -- from a template, then slides their eight
/// corners 2000 units along x once game flag 0x47 is set: at once in state 0,
/// which then kills the task, or from state 1 when the flag turns positive
/// later.
void func_dryfield_night_factory_8017F734(Task* task)
{
    GpGridParams* src = &D_dryfield_night_factory_80186C20;
    GpGridParams* geom;
    s32           i;

    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_night_factory_80187BF0;
    }
    switch (task->state) {
        case 0:
            for (i = 0; i < 2; i++) {
                geom->field_4[i].vx         = src->field_4[i].vx;
                geom->field_4[i].vy         = src->field_4[i].vy;
                geom->field_4[i].vz         = src->field_4[i].vz;
                geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
                geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
                geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
                geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
                geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
                geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
                geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
                geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
                geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
                geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
                geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
                geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
                geom->field_C[i]            = src->field_C[i];
            }
            if (GameFlag_GetNibble(0x47) != 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case 1:
            if (GameFlag_GetNibble(0x47) > 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
}

/// Runs the factory model for the bit of game flag 0x49 the task last saw: bit
/// 1 picks the first handler pair and bit 0 the second of the pair, the frame
/// counter at `NightFactoryWork::field_14` is bumped, and the model's coordinate
/// is rebuilt and handed to `func_800D7A9C` together with its translation.
static void func_dryfield_night_factory_8017FA08(Task* task)
{
    GfxCoord*         coord;
    NightFactoryWork* work;
    TmdObject*        obj;
    s32               flag;
    s32               prev;

    /* The model pointer is read twice on purpose: the second read is what
       leaves the target's `move s4, v0` copy. */
    coord = task->extra.tmd->coords;
    work  = (NightFactoryWork*)task->work;
    obj   = task->extra.tmd;
    flag  = GameFlag_GetNibble(0x49);
    prev  = work->field_0;
    if (flag != prev) {
        if ((flag ^ prev) & 1) {
            work->field_16 = 0;
        }
        if ((flag ^ work->field_0) & 2) {
            work->field_17 = 0;
        }
        work->field_0  = flag;
        work->field_14 = 0;
    }
    if (flag & 2) {
        func_dryfield_night_factory_8017E13C(task);
        if (flag & 1) {
            func_dryfield_night_factory_8017DA54(task);
        } else {
            func_dryfield_night_factory_8017DDD4(task);
        }
    } else {
        func_dryfield_night_factory_8017E480(task);
        if (flag & 1) {
            func_dryfield_night_factory_8017E7A4(task);
        } else {
            func_dryfield_night_factory_8017EBD4(task);
        }
    }
    work->field_14++;
    func_dryfield_night_factory_8017D858(task, 0, flag & 1);
    Gp_UpdateCoord(coord);
    func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
}

/// Kills the task; the factory model's exit callback.
static void func_dryfield_night_factory_8017FB48(Task* task)
{
    taskKill(task);
}

/// Binds the model to the light and colour matrices in the task's work block
/// and rebuilds its lighting.
static void func_dryfield_night_factory_8017FB68(Task* task)
{
    GfxCoord*         coord;
    NightFactoryWork* work;
    TmdObject*        extra;

    work                = (NightFactoryWork*)task->work;
    extra               = task->extra.tmd;
    coord               = extra->coords;
    extra->lightMtx     = &work->light;
    extra->colorMtx     = &work->color;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

/// Sends message 0x13F3 to `task`, if there is one.
static void func_dryfield_night_factory_8017FBC8(Task* arg0)
{
    if (arg0 != NULL) {
        Gp_DispatchMsg(arg0, 0x13F3, 0, 0);
    }
}

/// State 0 of the room's cutscene task: allocates the cutscene work block into
/// `Task::work` and dresses the task's model after the factory model in
/// `Task::spawnArg2`. Both bits of the model's flags follow the factory
/// model's, the root coordinate is seeded with a fixed offset under the
/// factory model's coordinate, and the factory model's light and colour
/// matrices are shared. A nibble of 1 in game flag 0x4E means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The factory task then adopts this one, which steps
/// on.
static void func_dryfield_night_factory_8017FBF4(Task* task)
{
    Task*                     cap      = task->spawnArg2.pointer;
    TmdObject*                model    = task->extra.tmd;
    TmdObject*                capModel = cap->extra.tmd;
    GfxCoord*                 coord    = model->coords;
    GfxCoord*                 capCoord = capModel->coords;
    NightFactoryCutsceneWork* work     = memCalloc(0xC, 0);
    u16                       flags;

    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    flags        = model->flags | TMD_OBJECT_HIDDEN;
    model->flags = flags;
    if (!(capModel->flags & TMD_OBJECT_HIDDEN)) {
        model->flags = flags & 0xFF7F;
    }
    if (!(capModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(model);
    } else {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    model->otOffset   = -1;
    coord->coord.t[1] = -0x316;
    coord->parent     = capCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (GameFlag_GetNibble(0x4E) == 1) {
        work->state = 0xFF;
        RotMatrixX(-0x300, &coord->coord);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx     = capModel->lightMtx;
    model->colorMtx     = capModel->colorMtx;
    Task_Reparent(cap, task);
    task->state += 1;
}

/// Runs the current state of the room's cutscene sequence, copying the room's
/// three handlers onto the stack first so the call goes through a local table
/// rather than through `.rodata`. A handler returning non-zero has finished its
/// part of the scene, which drops the sequence back to the shared state 0.
static void func_dryfield_night_factory_8017FD5C(Task* task)
{
    NightFactoryCutsceneWork*  work = (NightFactoryCutsceneWork*)task->work;
    NightFactoryCutsceneTable3 sp;

    sp = D_dryfield_night_factory_8017D5DC;
    if (sp.funcs[work->state](task) != 0) {
        work->state = 0;
    }
}

/// State 0 of the cutscene sequence: edge-detects game flag 0x4E and re-arms
/// the sequence when it changes. A nibble of 1 after a 0 moves the sequence to
/// state 1, and a nibble of 0 after a 1 moves it to state 2; either transition
/// restarts `step`. Every call records the nibble in `prevFlag`.
static s32 func_dryfield_night_factory_8017FDC8(Task* task)
{
    NightFactoryCutsceneWork* work  = (NightFactoryCutsceneWork*)task->work;
    s32                       flag  = GameFlag_GetNibble(0x4E);
    s32                       state = flag & 0xFF;

    if (state == 1) {
        if (work->prevFlag == 0) {
            work->state = state;
            goto reset;
        }
    }
    if (((flag & 0xFF) == 0) && (work->prevFlag == 1)) {
        work->state = 2;
    reset:
        work->step = 0;
    }
    work->prevFlag = flag;
    return 0;
}

/// Runs the factory model task's current state, through a copy of its handler
/// table on the stack.
void func_dryfield_night_factory_8017FE44(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D5C4;
    sp.funcs[task->state](task);
}

/// Runs the cutscene task's current state, through a copy of its handler table
/// on the stack.
void func_dryfield_night_factory_8017FE9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D5D0;
    sp.funcs[task->state](task);
}

/// Second cutscene driver for the night factory: silences both weapons, runs
/// the cap command in `Task::spawnArg1`, and once the cap reports event key 3
/// records progress flag 0x4A, restores the weapons and kills the task.
void func_dryfield_night_factory_8017FEF4(Task* task)
{
    s32 state = task->state;

    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_RunCapCmd(task->spawnArg1.value, 0);
            goto advance;
        case 1:
            if (GameFlag_GetNibble(0x4A) < 2) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            task->state++;
            /* fallthrough */
        case 2:
            if (Gp_CapBusy() != 0) {
                return;
            }
        advance:
            task->state++;
            return;
        case 3:
            if (Gp_GetCapEventKey() == state) {
                GameFlag_SetNibble(0x4A, 2);
            }
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
            taskKill(task);
            break;
    }
}

/// Runs the cap command in `Task::spawnArg1` unless game flag 0x47 is set, then
/// kills the task.
void func_dryfield_night_factory_80180038(Task* arg0)
{
    if (GameFlag_GetNibble(0x47) == 0) {
        Gp_RunCapCmd1(arg0->spawnArg1.value);
    }
    taskKill(arg0);
}

/// Plays the cutscene sequence out: sets game flag 0x4E, waits 0x3C frames,
/// runs the cap command in `Task::spawnArg1` and clears the flag again, waits
/// 0x1E frames, then hands the player and the ally their weapons back and kills
/// the task.
void func_dryfield_night_factory_8018007C(Task* task)
{
    switch (task->state) {
        case 0:
            GameFlag_SetNibble(0x4E, 1);
            task->killCountdown = 0x3C;
            task->state         = task->state + 1;
            return;
        case 2:
            Gp_RunCapCmd1(task->spawnArg1.value);
            GameFlag_SetNibble(0x4E, 0);
            task->killCountdown = 0x1E;
            task->state         = task->state + 1;
            return;
        case 1:
        case 3:
            if (--task->killCountdown < 0) {
                task->state = task->state + 1;
            }
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            taskKill(task);
            return;
    }
}
