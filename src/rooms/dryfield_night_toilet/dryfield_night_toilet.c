#include "rooms/dryfield_night_toilet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_toilet_8017DA70[];

/// The room's two glow-sprite points, one per camera view group that shows
/// one.
extern SVECTOR D_dryfield_night_toilet_8017DAA0[];
extern SVECTOR D_dryfield_night_toilet_8017DAA8[];

/// Gameplay's task descriptor table; the room task spawns its entry 0.
extern TaskDesc D_8013E51C[];

s32 func_dryfield_night_toilet_8017D5D0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_toilet_8017D644(Task*, s32, s32, s32);
s32 func_dryfield_night_toilet_8017D678(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_toilet_8017D680(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_toilet_8017D688(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_dryfield_night_toilet_8017DD88[1];
extern GpObj3A        D_dryfield_night_toilet_8017F2C4[1];
extern GpObj4C        D_dryfield_night_toilet_8017EE9C[6];
extern GpObj4C        D_dryfield_night_toilet_8017F064[8];
extern GpRoomCoordSet D_dryfield_night_toilet_8017EE84[1];

GpMsgEntry D_dryfield_night_toilet_8017DA70[6] = {
    { 5102, func_dryfield_night_toilet_8017D5D0 },
    { 5105, func_dryfield_night_toilet_8017D678 },
    { 5103, func_dryfield_night_toilet_8017D688 },
    { 5104, func_dryfield_night_toilet_8017D680 },
    { 5106, func_dryfield_night_toilet_8017D644 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_dryfield_night_toilet_8017DAA0[1] = {
    { 0, -1830, 1140, 0 },
};

SVECTOR D_dryfield_night_toilet_8017DAA8[1] = {
    { 0, -1900, -850, 0 },
};

GpRoomCoordRec D_dryfield_night_toilet_8017DAB0[1] = {
    { D_dryfield_night_toilet_8017EE84, NULL },
};

GpRoomObjRec D_dryfield_night_toilet_8017DAB8[1] = {
    { D_dryfield_night_toilet_8017DD88, D_dryfield_night_toilet_8017EE9C, D_dryfield_night_toilet_8017F064, D_dryfield_night_toilet_8017F2C4 },
};

u8* D_dryfield_night_toilet_8017DAC8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_toilet_8017DACC[1] = {
    { { .bytes = { 9, 0 } } },
};

GpWarpRec D_dryfield_night_toilet_8017DAD0[1] = {
    { { .words = { 2048, -1660, 0, 1653 } }, { 0, 0, 0, 0 }, { .words = { 2048, -1660, 0, 1653 } }, { 0, 0, 0, 0 }, 0x53100002, 0x53100001, 0, 2, 0, 480 },
};

SVECTOR D_dryfield_night_toilet_8017DB08[7] = {
#include "assets/dryfield_night_toilet_collision_007C8_normals.inc"
};

SVECTOR D_dryfield_night_toilet_8017DB40[36] = {
#include "assets/dryfield_night_toilet_collision_007C8_verts.inc"
};

GpGridFace D_dryfield_night_toilet_8017DC60[18] = {
#include "assets/dryfield_night_toilet_collision_007C8_faces.inc"
};

s16 D_dryfield_night_toilet_8017DD38[36] = {
#include "assets/dryfield_night_toilet_collision_007C8_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_toilet_8017DD38[i])
s16* D_dryfield_night_toilet_8017DD80[2] = {
#include "assets/dryfield_night_toilet_collision_007C8_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_toilet_8017DD88[1] = {
    { NULL, D_dryfield_night_toilet_8017DB08, D_dryfield_night_toilet_8017DB40, D_dryfield_night_toilet_8017DC60, D_dryfield_night_toilet_8017DD80, 2970, 2300, 1, 2, 4000, 18 },
};

GpViewRec D_dryfield_night_toilet_8017DDAC[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 1524 },
    { { { { 4000, 0, -880 }, { -628, 2868, -2855 }, { 616, 2924, 2801 } }, { 2049, 2467, 57 } }, 230 },
    { { { { -3969, 0, -1008 }, { -446, 3672, 1757 }, { 904, 1813, -3559 } }, { 2094, 1926, -1868 } }, 230 },
    { { { { -4005, 0, 858 }, { 107, 4063, 502 }, { -851, 513, -3973 } }, { -515, 1179, -1469 } }, 207 },
    { { { { 3989, 0, 927 }, { -344, 3803, 1481 }, { -861, -1521, 3704 } }, { -488, 203, 1520 } }, 207 },
    { { { { 863, 0, -4003 }, { -3600, 1791, -776 }, { 1751, 3683, 377 } }, { -1314, 2467, 1776 } }, 207 },
    { { { { -152, 0, -4093 }, { -3445, 2211, 128 }, { 2210, 3447, -82 } }, { -1153, 2360, -518 } }, 207 },
    { { { { -977, 0, -3977 }, { -3279, 2317, 805 }, { 2250, 3377, -552 } }, { -1153, 2360, -1708 } }, 207 },
    { { { { 3428, 0, 2241 }, { -2064, 1593, 3158 }, { -871, -3773, 1333 } }, { -539, 184, -108 } }, 230 },
};

GpSprtCmd D_dryfield_night_toilet_8017DEF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_toilet_8017DF00[43] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 325, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 80, 392, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -88, 96, 350, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -80, 72, 300, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 72, 325, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 32, -120, 687, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 24, -120, 700, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -48, 716, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -120, 488, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 48, 528, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 88, 512, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 0, 540, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -56, 510, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, -120, 474, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -56, 454, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 475, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 48, 501, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 88, 508, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -120, 417, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, -56, 423, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 0, 440, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 48, 469, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 447, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -120, 377, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -56, 405, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 0, 420, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 48, 406, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 433, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 56, -120, 637, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -24, 676, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -120, 612, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -24, 656, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -16, 620, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -120, 584, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 80, -120, 559, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 80, -8, 588, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, 16, 705, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -32, 722, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 48, -120, 620, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 40, -120, 583, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 40, -40, 757, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -56, 541, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -120, 541, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_toilet_8017E25C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 38, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_toilet_8017E27C[61] = {
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 40, 0, 525, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 24, 16, 400, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 56, 650, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, 72, 625, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, -24, 800, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -40, 825, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 40, 16, 450, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, 40, 40, 375, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 40, 64, 300, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, 40, 80, 300, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 56, -16, 600, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, -40, 825, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -32, -96, 766, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 40, 761, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 40, 715, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 40, 660, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 40, 648, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 40, 603, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 566, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 40, 532, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, -8, 517, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, -8, 456, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -8, 410, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -64, 400, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, -64, 444, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -112, -64, 500, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -120, 378, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -136, -120, 419, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -112, -120, 471, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 40, 417, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, 40, 465, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 40, 492, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 64, 393, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, 64, 427, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 64, 465, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 64, 500, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 386, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 88, 404, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 440, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 88, 470, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 8, 510, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -88, -64, 527, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, -120, 491, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 8, 563, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -80, -64, 568, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, -120, 517, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, 0, 586, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -64, 599, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -120, 546, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -8, 623, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -72, 632, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -120, 590, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -16, 636, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -72, 658, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, -120, 625, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -24, 697, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -80, 713, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -120, 656, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -40, -32, 716, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, -80, 740, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -120, 693, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_toilet_8017E740[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 61, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_toilet_8017E758[13] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 56, -32, 575, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, 64, -104, 538, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, 72, -104, 520, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, 80, -112, 482, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 208 } }, 88, -112, 462, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 112, -120, 389, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 104, -120, 414, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 96, -120, 446, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 128, -120, 341, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 120, -120, 366, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 136, -120, 301, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 144, -120, 288, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, 152, -120, 275, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_toilet_8017E85C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_toilet_8017E874[42] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 112, 402, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -40, 48, 765, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -64, 40, 600, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 96, 397, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -8, 725, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -56, 700, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -8, 687, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -64, 637, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -16, 635, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, -16, 650, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -88, 577, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -80, 616, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -40, 600, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -104, 600, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -40, 575, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -56, 556, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -160, -120, 377, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -128, -120, 471, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -120, 506, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 525, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 24, 575, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, 24, 538, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, 16, 425, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 16, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, -64, 376, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -64, 354, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -160, 8, 350, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, 8, 375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -56, 477, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -56, 500, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -8, -24, 1006, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -8, 56, 950, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 836, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 955, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 0, 997, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 24, 985, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, -24, 821, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 16, 785, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -24, 883, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 24, 977, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 0, 1006, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -16, -24, 1020, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_toilet_8017EBBC[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 2, 0 } },
    { 30, 12, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_dryfield_night_toilet_8017EBEC[2] = {
    { { 150, 0, 168, 239 }, 875 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtCmd D_dryfield_night_toilet_8017EC00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_toilet_8017EC10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_toilet_8017EC20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_toilet_8017EC30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_toilet_8017EC40[9] = {
    { { .empty = D_dryfield_night_toilet_8017DEF0 }, D_dryfield_night_toilet_8017DEF0, NULL },
    { { .elements = D_dryfield_night_toilet_8017DF00 }, D_dryfield_night_toilet_8017E25C, NULL },
    { { .elements = D_dryfield_night_toilet_8017E27C }, D_dryfield_night_toilet_8017E740, NULL },
    { { .elements = D_dryfield_night_toilet_8017E758 }, D_dryfield_night_toilet_8017E85C, NULL },
    { { .elements = D_dryfield_night_toilet_8017E874 }, D_dryfield_night_toilet_8017EBBC, D_dryfield_night_toilet_8017EBEC },
    { { .empty = D_dryfield_night_toilet_8017EC00 }, D_dryfield_night_toilet_8017EC00, NULL },
    { { .empty = D_dryfield_night_toilet_8017EC10 }, D_dryfield_night_toilet_8017EC10, NULL },
    { { .empty = D_dryfield_night_toilet_8017EC20 }, D_dryfield_night_toilet_8017EC20, NULL },
    { { .empty = D_dryfield_night_toilet_8017EC30 }, D_dryfield_night_toilet_8017EC30, NULL },
};

GpLight D_dryfield_night_toilet_8017ECAC[1] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 10, -10, -10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } },
};

GpPointLight D_dryfield_night_toilet_8017ED04[4] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1150 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1700, 1500, 1300, { 0, 0 } }, 700, 3083 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -850 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1800, 1600, 1300, { 0, 0 } }, 903, 4284 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1720, -1000, -850 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1400, 1200, 1000, { 0, 0 } }, 1324, 2254 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1720, -1000, 1170 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1700, 1500, 1300, { 0, 0 } }, 1042, 5024 },
};

GpRoomCoordSet D_dryfield_night_toilet_8017EE84[1] = {
    { 1, D_dryfield_night_toilet_8017ECAC, 4, D_dryfield_night_toilet_8017ED04, 0, NULL },
};

GpObj4C D_dryfield_night_toilet_8017EE9C[6] = {
    { NULL, NULL, NULL, { 127, -992, 6, 0 }, { { -1024, -2016, 0, 0 }, { 1025, -2016, 0, 0 }, { -1024, 2016, 0, 0 }, { 1025, 2016, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2260, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 128, -1056, -96, 0 }, { { 1024, -2080, 0, 0 }, { -1024, -2080, 0, 0 }, { 1024, 2080, 0, 0 }, { -1024, 2080, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1184, -1088, -1568, 0 }, { { -157, -2112, -1018, 0 }, { 145, -2112, 1008, 0 }, { -157, 2112, -1018, 0 }, { 145, 2112, 1008, 0 } }, { 4052, 0, -606, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1120, -800, -1600, 0 }, { { 149, -1824, 1011, 0 }, { -152, -1824, -1014, 0 }, { 149, 1824, 1011, 0 }, { -152, 1824, -1014, 0 } }, { -4051, 0, 601, 0 }, { 0, 0, 4096, 0 }, 2079, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -1857, -736, 799, 0 }, { { -1004, -1760, 199, 0 }, { 1005, -1760, -199, 0 }, { -1004, 1760, 199, 0 }, { 1005, 1760, -199, 0 } }, { -799, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 2035, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -1857, -1072, 638, 0 }, { { 1016, -2096, -53, 0 }, { -1027, -2096, 45, 0 }, { 1016, 2096, -53, 0 }, { -1027, 2096, 45, 0 } }, { 196, 0, 4115, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 3, 129, 0 },
};

GpObj4C D_dryfield_night_toilet_8017F064[8] = {
    { NULL, NULL, NULL, { -1808, -48, 1728, 0 }, { { -656, 0, -256, 0 }, { 656, 0, -256, 0 }, { -656, 0, 256, 0 }, { 656, 0, 256, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 704, 0, 15, 19, 2, 0 },
    { NULL, NULL, NULL, { 729, -64, -1472, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 765, -64, -480, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 764, -64, 448, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 768, -64, 1408, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { -304, -64, 608, 0 }, { { -448, 0, -1472, 0 }, { 448, 0, -1472, 0 }, { -448, 0, 1472, 0 }, { 448, 0, 1472, 0 } }, { 0, 4100, 0, 0 }, { 4091, 0, -201, 0 }, 1536, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -1152, -64, -480, 0 }, { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 523, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { -1824, -64, -1392, 0 }, { { -320, 0, -624, 0 }, { 320, 0, -624, 0 }, { -320, 0, 624, 0 }, { 320, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 701, 2, 10, 0, 130, 0 },
};

GpObj3A D_dryfield_night_toilet_8017F2C4[1] = {
    { NULL, NULL, { -864, -1488, 624, 0 }, { { 0, 1904, -1520, 0 }, { 0, -1904, -1520, 0 }, { 0, 1904, 1520, 0 }, { 0, -1904, 1520, 0 } }, { 4109, 0, 0, 0 }, { 124, 9 }, 129, 0 },
};

GpAreaTmdRec D_dryfield_night_toilet_8017F300[2] = {
    { 40, 40, 0, 0, { 0, 0 }, D_8013E500 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_toilet_8017F318[3] = {
    { 7, 7, 0, 0, { 0, 0 }, D_80138C80 },
    { 8, 7, 0, 0, { 0, 0 }, D_801393C8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_toilet_8017F33C[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_toilet_8017F354[12] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017BCC8, D_dryfield_night_toilet_8017F300 },
    { NULL, NULL },
    { D_map_dryfield_full_8017BD38, D_dryfield_night_toilet_8017F318 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017BDF8, D_dryfield_night_toilet_8017F33C },
};

s32 D_dryfield_night_toilet_8017F3B4[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_night_toilet_8017F3C0[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_toilet_8017F3C8[1] = {
    { 0, 0, 1, 0, D_dryfield_night_toilet_8017F3B4 },
};

GpRoomParamRec D_dryfield_night_toilet_8017F3D0[1] = {
    { 0, 1, 0, 0, D_dryfield_night_toilet_8017F3B4 },
};

GpRoomParamRec* D_dryfield_night_toilet_8017F3D8[8] = {
    D_dryfield_night_toilet_8017F3C0,
    D_dryfield_night_toilet_8017F3C8,
    D_dryfield_night_toilet_8017F3D0,
    D_dryfield_night_toilet_8017F3C0,
    D_dryfield_night_toilet_8017F3C0,
    D_dryfield_night_toilet_8017F3C0,
    D_dryfield_night_toilet_8017F3C0,
    D_dryfield_night_toilet_8017F3C0,
};

static void func_dryfield_night_toilet_8017D690(Task* task);
static void func_dryfield_night_toilet_8017D71C(Task* task);
static void func_dryfield_night_toilet_8017D77C(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0xF that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one. Returns 1.
s32 func_dryfield_night_toilet_8017D5D0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->prefix.packed == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}

/// Message-table handler for id 0x13F2: on event 5 queues stage sound
/// 0x52100005. Returns 0.
s32 func_dryfield_night_toilet_8017D644(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x52100000 | 5, 0, 0);
    }
    return 0;
}

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_toilet_8017D678(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_night_toilet_8017D680(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_toilet_8017D688(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// First state of the room task: publishes the room's message table and claims
/// pointer slot 7. When game nibble 0xAF is still clear and the session's place
/// (`gGameSession->at4.loc.place`) is 1, it sets the nibble to 1 and spawns
/// entry 0 of `D_8013E51C`. Advances to the next state either way.
static void func_dryfield_night_toilet_8017D690(Task* task)
{
    task->msgTable = D_dryfield_night_toilet_8017DA70;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xAF) == 0 && gGameSession->at4.loc.place == 1) {
        GameFlag_SetNibble(0xAF, 1);
        Task_SpawnFromTable(D_8013E51C, 0, 0, 0);
    }
    task->state = task->state + 1;
}

/// Second state of the room task: the room has nothing to do each frame.
static void func_dryfield_night_toilet_8017D71C(Task* task)
{
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_toilet_8017D5C4 = {
    { func_dryfield_night_toilet_8017D690, func_dryfield_night_toilet_8017D71C, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_toilet_8017D5C4`.
void func_dryfield_night_toilet_8017D724(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_toilet_8017D5C4;
    sp.funcs[task->state](task);
}

/// Queues a flickering sprite at the world point `arg0`: a semi-transparent
/// `POLY_FT4` square centred on the point's projection, with half-width
/// `arg2 * 39 / otz`, textured from the 40-texel cell `arg1` of tpage 0x2B and
/// shaded 0x20 or 0x30 on alternate frames. Points closer than OTZ 0x11 are
/// skipped.
static void func_dryfield_night_toilet_8017D77C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw25Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz > 0x10) {
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// Draws the room's glow sprite (texture cell 1, half-extent 0x200) at the
/// point the current camera view (`gGameSession->at4.loc.view`) shows: view 4
/// uses the second point, views 5 and 9 the first, and every other view draws
/// nothing.
void func_dryfield_night_toilet_8017D9F8(Task* unused)
{
    switch (gGameSession->at4.loc.view) {
        case 4:
            func_dryfield_night_toilet_8017D77C(&D_dryfield_night_toilet_8017DAA8[0], 1, 0x200);
            break;
        case 5:
        case 9:
            func_dryfield_night_toilet_8017D77C(&D_dryfield_night_toilet_8017DAA0[0], 1, 0x200);
            break;
    }
}
