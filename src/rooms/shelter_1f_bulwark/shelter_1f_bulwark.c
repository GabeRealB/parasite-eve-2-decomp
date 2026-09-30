#include "rooms/shelter_1f_bulwark.h"

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
#include "gameplay/loading.h"
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
#include "main/fs.h"
#include "main/fs_types.h"
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
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_1f_bulwark_80180ECC[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_1f_bulwark_80180ECC_value __asm__("D_shelter_1f_bulwark_80180ECC");

extern TaskDesc         D_shelter_1f_bulwark_80180320;
extern GpMsgEntry       D_shelter_1f_bulwark_8018032C[];
extern TaskDesc         D_shelter_1f_bulwark_80180354;
extern TaskDesc         D_shelter_1f_bulwark_80180360[];
extern SVECTOR          D_shelter_1f_bulwark_80180378[];
extern SVECTOR          D_shelter_1f_bulwark_80180398[];
extern GpFadeWork       D_shelter_1f_bulwark_80180EBC;
extern GpFadeWork       D_shelter_1f_bulwark_80180EC0;
extern RoomEventMsg     D_shelter_1f_bulwark_80180EC4;
extern RoomLatchedEvent D_shelter_1f_bulwark_80180ED0;

static void func_shelter_1f_bulwark_8017DBD4(Task* task);
static void func_shelter_1f_bulwark_8017DC18(Task* task);
static void func_shelter_1f_bulwark_8017E630(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_1f_bulwark_8017EA5C(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_1f_bulwark_8017F2E0(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_1f_bulwark_8017F960(GfxCoord* arg0, s16 arg1, u8* arg2);

extern GpGridParams   D_shelter_1f_bulwark_80180648[1];
extern GpObj3A        D_shelter_1f_bulwark_80180E08[2];
extern GpObj4C        D_shelter_1f_bulwark_80180A8C[2];
extern GpObj4C        D_shelter_1f_bulwark_80180B24[8];
extern GpRoomCoordSet D_shelter_1f_bulwark_80180A74[1];

s32  func_shelter_1f_bulwark_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_1f_bulwark_8017DBBC(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_1f_bulwark_8017DBC4(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_1f_bulwark_8017DBCC(Task*, s32, GpMessageArg, GpMessageArg);
void func_shelter_1f_bulwark_8017D61C(Task*);
void func_shelter_1f_bulwark_8017DA60(Task*);
void func_shelter_1f_bulwark_8017DC78(Task*);
void func_shelter_1f_bulwark_8017DE04(Task*);

TaskDesc D_shelter_1f_bulwark_80180320 = { 0, 32, func_shelter_1f_bulwark_8017D61C, { .model = NULL } };

GpMsgEntry D_shelter_1f_bulwark_8018032C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_bulwark_8017D7B4 },
    { 5105, func_shelter_1f_bulwark_8017DBBC },
    { 5103, func_shelter_1f_bulwark_8017DBCC },
    { 5104, func_shelter_1f_bulwark_8017DBC4 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_1f_bulwark_80180354 = { 0, 32, func_shelter_1f_bulwark_8017DA60, { .model = NULL } };

TaskDesc D_shelter_1f_bulwark_80180360[2] = {
    { 0, 192, func_shelter_1f_bulwark_8017DE04, { .model = NULL } },
    { 0, 192, func_shelter_1f_bulwark_8017DC78, { .model = NULL } },
};

SVECTOR D_shelter_1f_bulwark_80180378[4] = {
    { 3560, -6380, 2770, 0 },
    { 3560, -5130, -2810, 0 },
    { 2910, -4050, 1890, 0 },
    { 2910, -4050, -1930, 0 },
};

SVECTOR D_shelter_1f_bulwark_80180398[1] = {
    { -4180, -1940, 2720, 0 },
};

SVECTOR D_shelter_1f_bulwark_801803A0[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_shelter_1f_bulwark_801803B0[1] = {
    { D_shelter_1f_bulwark_80180648, D_shelter_1f_bulwark_80180A8C, D_shelter_1f_bulwark_80180B24, D_shelter_1f_bulwark_80180E08 },
};

GpRoomCoordRec D_shelter_1f_bulwark_801803C0[1] = {
    { D_shelter_1f_bulwark_80180A74, NULL },
};

u8* D_shelter_1f_bulwark_801803C8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_bulwark_801803CC[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_shelter_1f_bulwark_801803D0[2] = {
    { { .words = { 3072, 4000, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 3072, 3130, 0, 0 } }, { 0, 0, 0, 0 }, 0x55030002, 0x55030001, 0, 2, 0, 428 },
    { { .words = { 1024, -3800, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 3072, -3800, 0, 0 } }, { 0, 0, 0, 0 }, 0x55030004, 0x55030003, 0, 3, 0, 0 },
};

SVECTOR D_shelter_1f_bulwark_80180440[6] = {
#include "assets/shelter_1f_bulwark_collision_03088_normals.inc"
};

SVECTOR D_shelter_1f_bulwark_80180470[22] = {
#include "assets/shelter_1f_bulwark_collision_03088_verts.inc"
};

GpGridFace D_shelter_1f_bulwark_80180520[12] = {
#include "assets/shelter_1f_bulwark_collision_03088_faces.inc"
};

s16 D_shelter_1f_bulwark_801805B0[64] = {
#include "assets/shelter_1f_bulwark_collision_03088_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_1f_bulwark_801805B0[i])
s16* D_shelter_1f_bulwark_80180630[6] = {
#include "assets/shelter_1f_bulwark_collision_03088_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_1f_bulwark_80180648[1] = {
    { NULL, D_shelter_1f_bulwark_80180440, D_shelter_1f_bulwark_80180470, D_shelter_1f_bulwark_80180520, D_shelter_1f_bulwark_80180630, 4250, 3500, 3, 2, 4000, 12 },
};

GpViewRec D_shelter_1f_bulwark_8018066C[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x38D6, 0 } }, 257 },
    { { { { 888, 0, -3998 }, { 1332, 3861, 296 }, { 3769, -1364, 837 } }, { 2810, 260, 870 } }, 257 },
    { { { { 902, 0, 3995 }, { 3827, 1174, -864 }, { -1145, 3924, 258 } }, { 1110, 5890, 370 } }, 207 },
};

SpriteBatch D_shelter_1f_bulwark_801806D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_bulwark_801806E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_1f_bulwark_801806F8[8] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 32, 1225, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -104, 40, 1225, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -120, 48, 1225, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 48, 1400, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 64, 1400, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 96, 1425, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -112, 64, 1225, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -112, 96, 1425, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_bulwark_80180798[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_1f_bulwark_801807B0[3] = {
    { { .empty = D_shelter_1f_bulwark_801806D8 }, D_shelter_1f_bulwark_801806D8, NULL },
    { { .empty = D_shelter_1f_bulwark_801806E8 }, D_shelter_1f_bulwark_801806E8, NULL },
    { { .elements = D_shelter_1f_bulwark_801806F8 }, D_shelter_1f_bulwark_80180798, NULL },
};

GpPointLight D_shelter_1f_bulwark_801807D4[7] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3994, -1930, 2718 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 0, 0, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2500, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2293, 2048, { 0, 0 } }, 2500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2500, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2293, 2048, { 0, 0 } }, 2500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2293, 2048, { 0, 0 } }, 3000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -3000, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2293, 2048, { 0, 0 } }, 3000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -6000, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 1638, 1228, { 0, 0 } }, 6000, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -6000, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 1638, 1228, { 0, 0 } }, 6000, 7000 },
};

GpRoomCoordSet D_shelter_1f_bulwark_80180A74[1] = {
    { 0, NULL, 7, D_shelter_1f_bulwark_801807D4, 0, NULL },
};

GpObj4C D_shelter_1f_bulwark_80180A8C[2] = {
    { NULL, NULL, NULL, { -620, -3248, 96, 0 }, { { -313, -3712, -2468, 0 }, { 310, -3712, 2465, 0 }, { -313, 3712, -2468, 0 }, { 310, 3712, 2465, 0 } }, { 4075, 0, -516, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -478, -3312, 191, 0 }, { { 291, -3712, 2444, 0 }, { -302, -3712, -2455, 0 }, { 291, 3712, 2444, 0 }, { -302, 3712, -2455, 0 } }, { -4074, 0, 492, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 3, 2, 129, 0 },
};

GpObj4C D_shelter_1f_bulwark_80180B24[8] = {
    { NULL, NULL, NULL, { 3696, -53, 0, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1492, 0, 2, 19, 2, 0 },
    { NULL, NULL, NULL, { -3776, -53, 112, 0 }, { { -496, 0, -1776, 0 }, { 496, 0, -1776, 0 }, { -496, 0, 1776, 0 }, { 496, 0, 1776, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 1841, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 656, -64, -1200, 0 }, { { -3552, 0, -368, 0 }, { 3552, 0, -368, 0 }, { -3552, 0, 368, 0 }, { 3552, 0, 368, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 3565, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { -304, -64, 992, 0 }, { { -1744, 0, -368, 0 }, { 1744, 0, -368, 0 }, { -1744, 0, 368, 0 }, { 1744, 0, 368, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, -4096, 0 }, 1778, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -1600, -64, 2144, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1492, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -2656, -64, -2176, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1492, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { -3808, -64, -2704, 0 }, { { -496, 0, -496, 0 }, { 496, 0, -496, 0 }, { -496, 0, 496, 0 }, { 496, 0, 496, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 701, 0, 4, 33, 2, 0 },
    { NULL, NULL, NULL, { 2848, -64, 1024, 0 }, { { -1328, 0, -368, 0 }, { 1328, 0, -368, 0 }, { -1328, 0, 368, 0 }, { 1328, 0, 368, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1372, 2, 3, 0, 130, 0 },
};

GpAreaTmdRec D_shelter_1f_bulwark_80180D84[3] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_1f_bulwark_80180DA8[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AF00, D_shelter_1f_bulwark_80180D84 },
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

GpObj3A D_shelter_1f_bulwark_80180E08[2] = {
    { NULL, NULL, { 1840, -16, 2576, 0 }, { { -2864, 1936, -1104, 0 }, { 2864, 1936, 1104, 0 }, { -2864, -1936, -1104, 0 }, { 2864, -1936, 1104, 0 } }, { -1477, 0, 3827, 0 }, { 36, 14 }, 1, 0 },
    { NULL, NULL, { 2624, 0, -3120, 0 }, { { -3216, 1936, 896, 0 }, { 3216, 1936, -896, 0 }, { -3216, -1936, 896, 0 }, { 3216, -1936, -896, 0 } }, { 1102, 0, 3957, 0 }, { 17, 15 }, 129, 0 },
};

s32 D_shelter_1f_bulwark_80180E80[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_shelter_1f_bulwark_80180E8C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_1f_bulwark_80180E94[1] = {
    { 0, 0, 1, 0, D_shelter_1f_bulwark_80180E80 },
};

GpRoomParamRec* D_shelter_1f_bulwark_80180E9C[8] = {
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E94,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
    D_shelter_1f_bulwark_80180E8C,
};

GpFadeWork D_shelter_1f_bulwark_80180EBC = { 0 };

GpFadeWork D_shelter_1f_bulwark_80180EC0 = { 0 };

RoomEventMsg D_shelter_1f_bulwark_80180EC4 = { 0 };

s8 D_shelter_1f_bulwark_80180ECC[4] = {
    0,
    33,
    -78,
    -119,
};

RoomLatchedEvent D_shelter_1f_bulwark_80180ED0;

static __inline__ s32 Bulwark_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_1f_bulwark_8017DF00(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

/// The room's event task, spawned by its message handler for a latched event.
/// State 0 runs the event's CAP command; state 1 waits for it to finish and,
/// when the event asks for it, starts helper task 0x31; states 2 and 3 play
/// the event's stage sound, if any, and wait for it to end; state 4 moves the
/// save location to the latched message's area, warp and room and hands over
/// to task type 0x11.
void func_shelter_1f_bulwark_8017D61C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_bulwark_80180ED0.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_bulwark_80180ED0.fade != 0) {
                    D_shelter_1f_bulwark_80180EBC.field_0 = 0;
                    D_shelter_1f_bulwark_80180EBC.field_1 = 0;
                    D_shelter_1f_bulwark_80180EBC.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_1f_bulwark_80180EBC);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_bulwark_80180ED0.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_bulwark_80180ED0.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_bulwark_80180ED0.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = (u8)D_shelter_1f_bulwark_80180EC4.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_1f_bulwark_80180EC4.warp;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_1f_bulwark_80180EC4.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 Bulwark_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_bulwark_80180ECC_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_1f_bulwark_80180EC4 = *dst;
            D_shelter_1f_bulwark_80180ED0 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_bulwark_80180320, 0, 0, 0);
            D_shelter_1f_bulwark_80180ECC_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_bulwark_8017D7B4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    RoomLatchedEvent event;

    *dst = *src;
    func_map_neo_ark_80179B14(src, dst);
    if (src->areaId == 4) {
        if (GameFlag_GetNibble(0x15D) == 0) {
            Gp_SpawnIfCapIdle(1, 0);
            return 2;
        }
        if (GameFlag_GetNibble(0x7A) < 6) {
            if (src->queryOnly == ROOM_EVENT_EXECUTE) {
                GameFlag_SetNibble(0x7A, 6);
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_1f_bulwark_80180354, 0, 0, 0);
            }
            return 0;
        }
        event.capCmd   = 1;
        event.stageSnd = 0x55030003;
        event.flagId   = 0;
        event.fade     = 1;
        return Bulwark_StartEvent(dst, &event);
    }
    if (src->areaId == 2) {
        event.capCmd   = 6;
        event.stageSnd = 0x55030001;
        event.flagId   = 0x15C;
        event.fade     = 0;
        return Bulwark_StartEvent(dst, &event);
    }
    return 1;
}

/// The controller task's states: set up, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_bulwark_8017D5D8 = {
    {
        func_shelter_1f_bulwark_8017DBD4,
        func_shelter_1f_bulwark_8017DC18,
        taskKill,
    },
};

void func_shelter_1f_bulwark_8017DA60(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(1, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            goto advance;
        case 2:
            D_shelter_1f_bulwark_80180EC0.field_0 = 0;
            D_shelter_1f_bulwark_80180EC0.field_1 = 0;
            D_shelter_1f_bulwark_80180EC0.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_1f_bulwark_80180EC0);
            arg0->killCountdown = 0;
            SndEvt_EnqueueType6(0x55030003, 0, 0);
            goto advance;
        case 3:
            arg0->killCountdown++;
            if (arg0->killCountdown < 0x1F) {
                break;
            }
            goto advance;
        case 5:
            GameFlag_SetNibble(0x7A, 6);
            Task_SpawnFromTable(D_shelter_1f_bulwark_80180360, 0, 0, 0);
        case 4:
        case 6:
        advance:
            arg0->state++;
            break;
        case 7:
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_1f_bulwark_8017DBBC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBC4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBCC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room's controller task: installs the room's message
/// table, registers the task in pointer slot 7 and advances to the idle state.
static void func_shelter_1f_bulwark_8017DBD4(Task* task)
{
    task->msgTable = D_shelter_1f_bulwark_8018032C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

static void func_shelter_1f_bulwark_8017DC18(Task* task)
{
}

/// The room's controller task: copies its three-entry state table (set up,
/// idle, kill) to the stack and runs the entry for the current state.
void func_shelter_1f_bulwark_8017DC20(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_bulwark_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_1f_bulwark_8017DC78(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
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
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot((u8*)&key, 0, 0);
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->movieReady == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_shelter_1f_bulwark_8017DE04(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Display_SpawnWithOt(D_shelter_1f_bulwark_80180360, 1, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            Gp_SpawnViewTasks();
            Gp_StateF0.field_4 = 1;
            /* fallthrough */
        case 1:
        case 2:
            arg0->state = arg0->state + 1;
            break;
        case 3:
            Mc_SaveData[0].state.at4.loc.stage = 5;
            Mc_SaveData[0].state.at4.loc.area  = 0x1A;
            Mc_SaveData[0].state.at4.loc.warp  = 1;
            Mc_SaveData[0].state.at4.loc.room  = 1;
            gDisplayState.spriteVariant        = 1;
            Fs_BeginBootLoad((u8*)&Mc_SaveData[0].state.at4.loc, 0);
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

/// Draws a flickering glow at the world-space point `worldPoint`: projects it through
/// the view matrix and, unless it lies too close to the camera, queues four
/// gouraud wedges around it whose centre vertex carries the colour and whose
/// rim is black. `radiusScale` is the glow's size, scaled down with depth. `packedColor`
/// packs the colour as three channel weights (bits 8 up, 4-5 and 0-1), each
/// multiplied by a brightness that alternates every frame.
///
/// `worldPoint` uses world coordinates; `radiusScale` is narrowed to signed 16 bits
/// before division by camera depth/4. Angles use 4096 units per turn and the
/// trigonometric coordinates use a 12-bit fractional scale. Colour bytes wrap.
static void func_shelter_1f_bulwark_8017DF00(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH       = 17,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE = 0x20,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP = 8,
        ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT      = 12,
        ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN       = 0x1000,
    };

    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    DisplayState*      displayBase;
    DisplayState*      ds;
    s32                radius;
    s32                angle;
    s32                halfStepAngle;
    s32                nextAngle;
    s32                shiftedColor;
    u8                 brightness;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(*block));
        block   = (RoomDraw25Scratch*)tmp;
    }

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - sizeof(*block)))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - sizeof(*block)))->otz >= ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * 64) / ((RoomDraw25Scratch*)(head - sizeof(*block)))->otz;
        displayBase   = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayBase->animFrame & 1) * ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP) | ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE;
        r             = brightness * (shiftedColor >> 24);
        g             = brightness * ((shiftedColor >> 20) & 3);
        b             = brightness * (packedColor & 3);
        angle         = 0;
        ds            = displayBase;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + 0x200;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            nextAngle     = angle + 0x400;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (angle < ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN);
    }
    SCRATCH_POP_BYTES(sizeof(*block));
}

void func_shelter_1f_bulwark_8017E2A4(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601C2;
        D_8011572C  = 0x601C3;
        D_80115750  = 0x601C4;
        arg0->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_bulwark_80180378;
            func_shelter_1f_bulwark_8017DF00(&p[0], 0x300, 0x210);
            func_shelter_1f_bulwark_8017DF00(&p[1], 0x300, 0x210);
            func_shelter_1f_bulwark_8017DF00(&p[2], 0x300, 0x111);
            func_shelter_1f_bulwark_8017DF00(&p[3], 0x300, 0x111);
            break;
        }
        case 3:
            func_shelter_1f_bulwark_8017DF00(&D_shelter_1f_bulwark_80180398[0], 0x200, 0x200);
            break;
    }
}

void func_shelter_1f_bulwark_8017E38C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
                func_shelter_1f_bulwark_8017EA5C(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_1f_bulwark_8017EA5C(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_1f_bulwark_8017E630(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_shelter_1f_bulwark_8017F960(coord, (s16)(work->angle * 3), rgb);
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

/// Draws a ring of sixteen gouraud segments around the coordinate's world
/// position, skipped when the projection fails. The ring runs from radius
/// `arg1`, black, out to `arg1 + arg2`, tinted with `rgb`; both radii shrink
/// with depth.
static void func_shelter_1f_bulwark_8017E630(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Draws a glow disc of eight gouraud wedges around the coordinate's world
/// position, skipped when the projection fails. The centre is tinted with
/// `rgb` and the rim is black; `arg1` is the radius before depth scaling.
static void func_shelter_1f_bulwark_8017EA5C(GfxCoord* arg0, s16 arg1, u8* rgb)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomFanScratch);
}

void func_shelter_1f_bulwark_8017EDF0(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_shelter_1f_bulwark_801803A0[0].vx;
                objCoord->coord.t[1]   = D_shelter_1f_bulwark_801803A0[0].vy;
                objCoord->coord.t[2]   = D_shelter_1f_bulwark_801803A0[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_shelter_1f_bulwark_801803A0[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_shelter_1f_bulwark_801803A0[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_1f_bulwark_8017F2E0(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a ribbon between two eight-slot rings of coordinates as seven gouraud
/// quads, walking back from slot `arg2`: each quad joins two consecutive slots
/// of `arg0` to the same slots of `arg1`, and fades as it gets older. `arg3`
/// packs the colour as channel weights (bits 8 up, 4-5 and 0-1). A quad whose
/// projection fails is skipped.
static void func_shelter_1f_bulwark_8017F2E0(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

void func_shelter_1f_bulwark_8017F6D8(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
            func_shelter_1f_bulwark_8017E630(objCoord, 0x100, 0x100, rgb);
            func_shelter_1f_bulwark_8017E630(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped flare at the coordinate's world position, skipped when
/// the projection fails: a disc of radius `arg1` in half-strength `arg2`
/// colour, a half-size disc in full colour, and four spikes, two of them twice
/// as long. Every wedge fades from its tinted centre to a black rim, and all
/// radii shrink with depth.
static void func_shelter_1f_bulwark_8017F960(GfxCoord* arg0, s16 arg1, u8* arg2)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
