#include "rooms/neo_ark_savanna_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern SVECTOR D_neo_ark_savanna_zone_8017F9D4[2];

extern TaskDesc         D_neo_ark_savanna_zone_8017F9A0;
extern GpSaveLoc        D_neo_ark_savanna_zone_80180990;
extern s8               D_neo_ark_savanna_zone_80180998;
extern RoomLatchedEvent D_neo_ark_savanna_zone_8018099C;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage D_neo_ark_savanna_zone_80180988;

/// The room's message table, which the room setup task installs.
extern GpMsgEntry D_neo_ark_savanna_zone_8017F9AC[];

/// The coordinate trail's two spawn offsets: `[0]` places the object's own
/// frame and `[1]` the second trail's frame. `D_neo_ark_savanna_zone_8017F9D4[1]`
/// is `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_savanna_zone_8017DCB0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_savanna_zone_8017E0DC(GpCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_savanna_zone_8017E960(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_savanna_zone_8017EFE0(GpCoord* arg0, s16 arg1, u8* arg2);

extern GpGridParams   D_neo_ark_savanna_zone_8017FBD0[1];
extern GpObj3A        D_neo_ark_savanna_zone_801808CC[1];
extern GpObj4C        D_neo_ark_savanna_zone_801804EC[4];
extern GpObj4C        D_neo_ark_savanna_zone_8018061C[5];
extern GpRoomBoundVec D_neo_ark_savanna_zone_80180908[5];
extern GpRoomCoordSet D_neo_ark_savanna_zone_801804D4[1];
extern TaskDesc       D_8014D8A4;
s32                   func_neo_ark_savanna_zone_8017D77C(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32                   func_neo_ark_savanna_zone_8017D8F0(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_neo_ark_savanna_zone_8017D8F8(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_neo_ark_savanna_zone_8017D900(Task*, s32, GpMessageArg, GpMessageArg);
void                  func_neo_ark_savanna_zone_8017D5E4(Task*);

TaskDesc D_neo_ark_savanna_zone_8017F9A0 = { 0, 32, func_neo_ark_savanna_zone_8017D5E4, { .model = NULL } };

GpMsgEntry D_neo_ark_savanna_zone_8017F9AC[5] = {
    { 5102, func_neo_ark_savanna_zone_8017D77C },
    { 5105, func_neo_ark_savanna_zone_8017D8F0 },
    { 5103, func_neo_ark_savanna_zone_8017D900 },
    { 5104, func_neo_ark_savanna_zone_8017D8F8 },
    { 0x7FFFFFFF, NULL },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_neo_ark_savanna_zone_8017F9D4[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_neo_ark_savanna_zone_8017F9E4[1] = {
    { D_neo_ark_savanna_zone_8017FBD0, D_neo_ark_savanna_zone_801804EC, D_neo_ark_savanna_zone_8018061C, D_neo_ark_savanna_zone_801808CC },
};

GpRoomCoordRec D_neo_ark_savanna_zone_8017F9F4[1] = {
    { D_neo_ark_savanna_zone_801804D4, D_neo_ark_savanna_zone_80180908 },
};

u8* D_neo_ark_savanna_zone_8017F9FC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_savanna_zone_8017FA00[1] = {
    { { .bytes = { 4, 0 } } },
};

GpWarpRec D_neo_ark_savanna_zone_8017FA04[2] = {
    { { .words = { 1024, 510, 0, 1500 } }, { 0, 0, 0, 0 }, { .words = { 1024, 510, 0, 1500 } }, { 0, 0, 0, 0 }, 0x55120002, 0x55120001, 0, 4, 0, 0 },
    { { .words = { 3072, 0x34BC, 0, 1500 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x34BC, 0, 1500 } }, { 0, 0, 0, 0 }, 0x55120004, 0x55120003, 0, 2, 0, 0 },
};

SVECTOR D_neo_ark_savanna_zone_8017FA74[5] = {
    { 4096, 0, 0, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, -4096, 0, 0 },
};

SVECTOR D_neo_ark_savanna_zone_8017FA9C[12] = {
    { -50, -3950, 2950, 0 },
    { -50, 50, 2950, 0 },
    { -50, 50, -50, 0 },
    { -50, -3950, -50, 0 },
    { 0x367E, -3950, -50, 0 },
    { 0x2A8A, 50, -50, 0 },
    { 0x367E, 50, -50, 0 },
    { 0x367E, 50, 2950, 0 },
    { 0x367E, -3950, 2950, 0 },
    { 5710, 50, 2950, 0 },
    { 0x29E0, 50, 2950, 0 },
    { 5450, 50, -50, 0 },
};

GpGridFace D_neo_ark_savanna_zone_8017FAFC[11] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 4, 5, 6, 0xFFFF }, 1, 0 },
    { { 6, 7, 4, 8 }, 2, 0 },
    { { 0, 9, 1, 0xFFFF }, 3, 1 },
    { { 10, 7, 5, 6 }, 4, 2 },
    { { 0, 8, 9, 10 }, 3, 1 },
    { { 11, 2, 9, 1 }, 4, 2 },
    { { 9, 10, 11, 5 }, 4, 3 },
    { { 10, 8, 7, 0xFFFF }, 3, 1 },
    { { 11, 3, 2, 0xFFFF }, 1, 0 },
    { { 11, 5, 3, 4 }, 1, 0 },
};

s16 D_neo_ark_savanna_zone_8017FB80[8] = {
    0,
    3,
    5,
    6,
    7,
    9,
    10,
    -1,
};

s16 D_neo_ark_savanna_zone_8017FB90[7] = {
    3,
    5,
    6,
    7,
    9,
    10,
    -1,
};

s16 D_neo_ark_savanna_zone_8017FBA0[8] = {
    1,
    2,
    4,
    5,
    7,
    8,
    10,
    -1,
};

s16 D_neo_ark_savanna_zone_8017FBB0[8] = {
    1,
    2,
    4,
    5,
    7,
    8,
    10,
    -1,
};

s16* D_neo_ark_savanna_zone_8017FBC0[4] = {
    D_neo_ark_savanna_zone_8017FB80,
    D_neo_ark_savanna_zone_8017FB90,
    D_neo_ark_savanna_zone_8017FBA0,
    D_neo_ark_savanna_zone_8017FBB0,
};

GpGridParams D_neo_ark_savanna_zone_8017FBD0[1] = {
    { NULL, D_neo_ark_savanna_zone_8017FA74, D_neo_ark_savanna_zone_8017FA9C, D_neo_ark_savanna_zone_8017FAFC, D_neo_ark_savanna_zone_8017FBC0, 50, 50, 4, 1, 4000, 11 },
};

GpViewRec D_neo_ark_savanna_zone_8017FBF4[4] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { -7500, 0x77D8, -1950 } }, 329 },
    { { { { -676, 0, -4039 }, { -670, 4039, 112 }, { 3983, 679, -667 } }, { -6330, 1950, -2240 } }, 329 },
    { { { { -365, 0, -4079 }, { -37, 4095, 3 }, { 4079, 38, -365 } }, { -1580, 1430, -2240 } }, 329 },
    { { { { -753, 0, 4026 }, { 207, 4090, 38 }, { -4020, 211, -752 } }, { -9980, 1620, -2260 } }, 329 },
};

GpSprtCmd D_neo_ark_savanna_zone_8017FC84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_savanna_zone_8017FC94[24] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 16, 825, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 16, 825, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 875, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -8, 875, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -32, 925, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -8, 1237, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -40, 1237, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, -40, 1237, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, -24, 1237, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, -72, 1237, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, -72, 1237, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, -96, 1237, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -104, 1237, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 8, 1025, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, 8, 1025, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 8, 1025, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -8, 1025, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 1025, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 962, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 48, 962, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 48, 962, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 16, 962, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 16, 962, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 8, 962, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_neo_ark_savanna_zone_8017FE74[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 8, 0, 0, { 0, 0 } },
    { 13, 5, 0, 0, { 2, 0 } },
    { 18, 6, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_savanna_zone_8017FEA4[35] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 625, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 625, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 625, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 625, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 625, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 0, 625, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 625, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -80, -80, 625, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 625, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, -120, 625, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 40, -120, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 40, -96, 625, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 96, 712, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 56, 712, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 56, 712, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 16, 712, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 16, 712, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -24, 712, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -24, 712, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -64, 712, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -64, 712, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -64, 712, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -104, 712, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -88, 712, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -120, 712, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, -120, 712, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -24, 1062, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -32, 1062, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 8, 1062, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 48, 1062, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -120, 48, 1062, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 8, 1062, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -120, -24, 1062, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -80, 24, 1062, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_neo_ark_savanna_zone_80180160[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 14, 0, 0, { 2, 0 } },
    { 27, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_savanna_zone_80180188[29] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 0, 1312, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 1312, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 1312, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 1312, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 0, 1312, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 1312, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 1312, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -40, 1312, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -40, 1312, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -64, 1312, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -120, 1312, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -96, 1312, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, -80, 1312, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 1700, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -8, 1700, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -48, 1700, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -72, 1700, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, -72, 1700, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -48, 1700, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -8, 1700, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 88, 32, 1700, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -32, 1700, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -64, 1700, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 1050, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 1050, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -8, 1050, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -24, 1050, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 8, 1050, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 40, 1050, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_neo_ark_savanna_zone_801803CC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 10, 0, 0, { 2, 0 } },
    { 23, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_savanna_zone_801803F4[4] = {
    { { .empty = D_neo_ark_savanna_zone_8017FC84 }, D_neo_ark_savanna_zone_8017FC84, NULL },
    { { .elements = D_neo_ark_savanna_zone_8017FC94 }, D_neo_ark_savanna_zone_8017FE74, NULL },
    { { .elements = D_neo_ark_savanna_zone_8017FEA4 }, D_neo_ark_savanna_zone_80180160, NULL },
    { { .elements = D_neo_ark_savanna_zone_80180188 }, D_neo_ark_savanna_zone_801803CC, NULL },
};

GpLight D_neo_ark_savanna_zone_80180424[2] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 952, -472, 229 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 5837, 5715, 5574, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -834, -528, -362 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4243, 4202, 4138, { 0, 0 } },
};

GpRoomCoordSet D_neo_ark_savanna_zone_801804D4[1] = {
    { 2, D_neo_ark_savanna_zone_80180424, 0, NULL, 0, NULL },
};

GpObj4C D_neo_ark_savanna_zone_801804EC[4] = {
    { NULL, NULL, NULL, { 5503, -2305, 1552, 0 }, { { 55, -2608, -3196, 0 }, { -59, -2608, 3190, 0 }, { 55, 2609, -3196, 0 }, { -59, 2609, 3190, 0 } }, { 4098, 0, 73, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 5727, -2305, 1744, 0 }, { { -59, -2608, 3336, 0 }, { 53, -2608, -3342, 0 }, { -59, 2608, 3336, 0 }, { 53, 2608, -3342, 0 } }, { -4095, 0, -69, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x277F, -2256, 1727, 0 }, { { -55, -2688, 3339, 0 }, { 56, -2688, -3338, 0 }, { -55, 2688, 3339, 0 }, { 56, 2688, -3338, 0 } }, { -4103, 0, -69, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 9951, -2224, 1695, 0 }, { { 57, -2688, -3193, 0 }, { -56, -2688, 3194, 0 }, { 57, 2688, -3193, 0 }, { -56, 2688, 3194, 0 } }, { 4095, 0, 72, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 2, 3, 129, 0 },
};

GpObj4C D_neo_ark_savanna_zone_8018061C[5] = {
    { NULL, NULL, NULL, { 736, -48, 1376, 0 }, { { -672, 0, -736, 0 }, { 672, 0, -736, 0 }, { -672, 0, 736, 0 }, { 672, 0, 736, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 995, 0, 21, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x33C0, -48, 1440, 0 }, { { -672, 0, -1136, 0 }, { 672, 0, -1136, 0 }, { -672, 0, 1136, 0 }, { 672, 0, 1136, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 1317, 0, 19, 33, 2, 0 },
    { NULL, NULL, NULL, { 7776, -64, 272, 0 }, { { -1440, 0, -432, 0 }, { 1440, 0, -432, 0 }, { -1440, 0, 432, 0 }, { 1440, 0, 432, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, 4096, 0 }, 1498, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 6992, -64, 256, 0 }, { { -6992, 0, -432, 0 }, { 6992, 0, -432, 0 }, { -6992, 0, 432, 0 }, { 6992, 0, 432, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 7001, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 6976, -64, 2720, 0 }, { { -6992, 0, -432, 0 }, { 6992, 0, -432, 0 }, { -6992, 0, 432, 0 }, { 6992, 0, 432, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 7001, 2, 4, 0, 130, 0 },
};

GpAreaTmdRec D_neo_ark_savanna_zone_80180798[3] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 25, 25, 2, 0, { 0, 0 }, D_801679A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_savanna_zone_801807BC[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_savanna_zone_801807D4[3] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 26, 26, 2, 0, { 0, 0 }, D_8016A8D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_savanna_zone_801807F8[3] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 25, 25, 2, 0, { 0, 0 }, D_801679A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_savanna_zone_8018081C[3] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 56, 56, 1, 0, { 0, 0 }, D_801602C0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_savanna_zone_80180840[3] = {
    { 56, 56, 0, 0, { 0, 0 }, D_801482C0 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_savanna_zone_80180864[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017BB70, D_neo_ark_savanna_zone_80180798 },
    { D_map_neo_ark_8017BBF0, D_neo_ark_savanna_zone_801807BC },
    { D_map_neo_ark_8017BC70, D_neo_ark_savanna_zone_801807D4 },
    { D_map_neo_ark_8017BCE0, D_neo_ark_savanna_zone_801807F8 },
    { D_map_neo_ark_8017BD50, D_neo_ark_savanna_zone_8018081C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BD80, D_neo_ark_savanna_zone_80180840 },
    { NULL, NULL },
};

GpObj3A D_neo_ark_savanna_zone_801808CC[1] = {
    { NULL, NULL, { 6928, -3008, 1552, 0 }, { { -7952, 0, -2576, 0 }, { 7952, 0, -2576, 0 }, { -7952, 0, 2576, 0 }, { 7952, 0, 2576, 0 } }, { 0, 4103, 0, 0 }, { 126, 32 }, 129, 0 },
};

GpRoomBoundVec D_neo_ark_savanna_zone_80180908[5] = {
    { 4, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 422, 394, 324, 395 },
    { 422, 394, 324, 395 },
    { 422, 394, 324, 395 },
};

s32 D_neo_ark_savanna_zone_80180930[3] = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

s32 D_neo_ark_savanna_zone_8018093C[3] = {
    0x10000031,
    0x10000033,
    0x10000031,
};

GpRoomParamRec D_neo_ark_savanna_zone_80180948[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_savanna_zone_80180950[1] = { 0 };

GpRoomParamRec D_neo_ark_savanna_zone_80180958[1] = {
    { 0, 0, 1, 0, D_neo_ark_savanna_zone_80180930 },
};

GpRoomParamRec D_neo_ark_savanna_zone_80180960[1] = {
    { 0, 0, 1, 0, D_neo_ark_savanna_zone_8018093C },
};

GpRoomParamRec* D_neo_ark_savanna_zone_80180968[8] = {
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180950,
    D_neo_ark_savanna_zone_80180958,
    D_neo_ark_savanna_zone_80180960,
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180948,
    D_neo_ark_savanna_zone_80180948,
};

RoomFadeStorage D_neo_ark_savanna_zone_80180988 = { 0 };

GpSaveLoc D_neo_ark_savanna_zone_80180990 = { 0 };

s8 D_neo_ark_savanna_zone_80180998 = 0;

RoomLatchedEvent D_neo_ark_savanna_zone_8018099C = { 0 };

static __inline__ s32 NeoArkSavannaZone_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event);
static void           func_neo_ark_savanna_zone_8017D908(Task* task);
static void           func_neo_ark_savanna_zone_8017D94C(Task* task);

/// The room's event task, spawned when the room latches an event. State 0 runs
/// the event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound, if any, and wait for it; state 4 writes the latched destination into
/// the save data and hands over to task type 0x11.
void func_neo_ark_savanna_zone_8017D5E4(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_neo_ark_savanna_zone_8018099C.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_neo_ark_savanna_zone_8018099C.fade != 0) {
                    D_neo_ark_savanna_zone_80180988.fade.field_0 = 0;
                    D_neo_ark_savanna_zone_80180988.fade.field_1 = 0;
                    D_neo_ark_savanna_zone_80180988.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_neo_ark_savanna_zone_80180988.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_neo_ark_savanna_zone_8018099C.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_neo_ark_savanna_zone_8018099C.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_neo_ark_savanna_zone_8018099C.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant         = 1;
            Mc_SaveData[0].state.at4.loc.area = D_neo_ark_savanna_zone_80180990.prefix.bytes.field_0;
            Mc_SaveData[0].state.at4.loc.warp = D_neo_ark_savanna_zone_80180990.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_neo_ark_savanna_zone_80180990.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Latches the room's pending event and starts the controller that runs it:
/// clears the "event running" flag, and once the event's flag nibble is clear
/// (or the event carries no flag) and `dst->field_5` does not ask for the side
/// effects to be suppressed, commits `dst` and the event and spawns the
/// controller task. Answers 2 for a started event, 1 when `field_5` held it
/// back.
static __inline__ s32 NeoArkSavannaZone_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event)
{
    D_neo_ark_savanna_zone_80180998 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_neo_ark_savanna_zone_80180990 = *dst;
            D_neo_ark_savanna_zone_8018099C = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_neo_ark_savanna_zone_8017F9A0, 0, 0, 0);
            D_neo_ark_savanna_zone_80180998 = 1;
        }
        return 2;
    }
    return 1;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `func_map_neo_ark_80179B14`. Messages 0x13 and
/// 0x15 build the room's event record - cap command 3 / 2, the stage sound and
/// flag 0x15E / 0x15F - and hand it to `NeoArkSavannaZone_StartEvent`; every
/// other message is not consumed and answers 1.
s32 func_neo_ark_savanna_zone_8017D77C(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    RoomLatchedEvent event;
    s32              cmd;
    s32              snd;
    s16              flag;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (*(u16*)in != 0x13) {
        goto message15;
    }
    snd            = 0x55120003;
    cmd            = 3;
    event.stageSnd = snd;
    flag           = 0x15E;
start_event:
    event.capCmd = cmd;
    event.flagId = flag;
    event.fade   = 0;
    return NeoArkSavannaZone_StartEvent(out, &event);
message15:
    if (*(u16*)in == 0x15) {
        snd            = 0x55120001;
        cmd            = 2;
        event.stageSnd = snd;
        flag           = 0x15F;
        goto start_event;
    }
    return 1;
}

s32 func_neo_ark_savanna_zone_8017D8F0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_savanna_zone_8017D8F8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_savanna_zone_8017D900(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room setup task: installs the room's message table and
/// pointer slot 7, then advances state.
static void func_neo_ark_savanna_zone_8017D908(Task* task)
{
    task->msgTable = D_neo_ark_savanna_zone_8017F9AC;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room setup task: does nothing, so the task idles there.
static void func_neo_ark_savanna_zone_8017D94C(Task* task)
{
}

/// State table of the room setup task, indexed by `Task::state`.
static const TaskFuncTable3 D_neo_ark_savanna_zone_8017D5D8 = { {
    func_neo_ark_savanna_zone_8017D908,
    func_neo_ark_savanna_zone_8017D94C,
    taskKill,
} };

/// The room setup task: runs the state handler its state selects, through a
/// copy of the state table on the stack.
void func_neo_ark_savanna_zone_8017D954(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_savanna_zone_8017D5D8;
    sp.funcs[task->state](task);
}

/// Room effect task: on its first run stores 0x601DD, 0x601F9 and 0x60215 in
/// three gameplay globals - values of the form `Gp_SpawnEff` takes as effect
/// ids - and sets `Gp_State1C->roomEffectMode` to 2.
void func_neo_ark_savanna_zone_8017D9AC(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758                 = 0x601DD;
        D_8011572C                 = 0x601F9;
        D_80115750                 = 0x60215;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }
}

/// Expanding flash burst. State 0 derives the per-frame step from the spawn
/// argument (the frame count). State 1 grows the level and radius each frame,
/// drawing a disc at the radius, a half-bright one at twice it and a ring
/// closing in from 0x300, and on its last frame lays a full-screen fade quad.
/// State 2 draws a star glow at three times the radius while the level falls
/// back to 0x10, then releases the work block. While the room's event state is
/// set it draws nothing, and releases the block once that reaches 4.
void func_neo_ark_savanna_zone_8017DA0C(Task* task)
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
                func_neo_ark_savanna_zone_8017E0DC(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_savanna_zone_8017E0DC(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_savanna_zone_8017DCB0(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_neo_ark_savanna_zone_8017EFE0(coord, (s16)(work->angle * 3), rgb);
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

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` segments
/// forming a ring between two radii, `arg1` and `arg1 + arg2` in world units
/// scaled by depth. The edge at `arg1` is black and the edge at `arg1 + arg2`
/// carries `rgb`, so the ring fades out towards `arg1`.
static void func_neo_ark_savanna_zone_8017DCB0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
static void func_neo_ark_savanna_zone_8017E0DC(GpCoord* arg0, s16 arg1, u8* rgb)
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

/// Twin coordinate trail. State 0 allocates sixteen `GpCoord`s, eight
/// per trail, and seeds them all from the two spawn offsets so each trail
/// starts collapsed on its origin. State 1 advances one slot of each trail per
/// frame, re-derives all sixteen against the view and draws them. The task
/// frees itself once its age reaches the spawn argument, and idles while the
/// room's event state is 2 or more.
void func_neo_ark_savanna_zone_8017E470(Task* task)
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
                objCoord->coord.t[0] = D_neo_ark_savanna_zone_8017F9D4[0].vx;
                objCoord->coord.t[1] = D_neo_ark_savanna_zone_8017F9D4[0].vy;
                objCoord->coord.t[2] = D_neo_ark_savanna_zone_8017F9D4[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_neo_ark_savanna_zone_8017F9D4[1];
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
                coord.sub = work->parent;
                {
                    SVECTOR* edge    = &D_neo_ark_savanna_zone_8017F9D4[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.flg = 0;
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
                func_neo_ark_savanna_zone_8017E960(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the trail
/// colour, packed as red from bit 8 up, green in bits 4-5 and blue in bits
/// 0-1, each multiplying that fade. A quad is dropped when `gte_stflg` is
/// negative.
static void func_neo_ark_savanna_zone_8017E960(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
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
        i0           = (arg2 - i) & 7;
        i1           = (arg2 - i - 1) & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        blk->v[0].vy = a->workm.t[1];
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
            blk->otz       = blk->otz + 1;
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            lo             = (fade - 9) & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            b2             = lo * (arg3 & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
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

/// Spark burst. State 0 fires the burst's effect, then a non-zero spawn
/// argument starts a stream of jittered sparks (state 1) and a zero one a pair
/// of rings whose radius grows and brightness falls each frame (state 2).
/// Either way the task reaches state 3 after seven frames and releases its
/// work block, or earlier once the room's event state reaches 4.
void func_neo_ark_savanna_zone_8017ED58(Task* task)
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
            func_neo_ark_savanna_zone_8017DCB0(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_savanna_zone_8017DCB0(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges: sixteen around the circle, alternating full radius at half
/// intensity and half radius at full intensity, then four spikes a quarter
/// turn apart, two reaching the full radius and two twice it. `arg1` sizes it
/// in world units scaled by depth; every wedge fades to black at its rim.
static void func_neo_ark_savanna_zone_8017EFE0(GpCoord* arg0, s16 arg1, u8* arg2)
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
