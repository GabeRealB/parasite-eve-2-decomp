#include "rooms/neo_ark_r26.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
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

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// Cutscene script blob argument of `func_800E8634`.
extern GpEvsCmd D_neo_ark_r26_8017DA74[];

/// Cutscene script blob argument of `func_800E8634`.
extern GpEvsCmd D_neo_ark_r26_8017DFCC[];

/// Room message handler table installed into `Task::msgTable`.
extern GpMsgEntry D_neo_ark_r26_8017E0A4[];

s32 func_neo_ark_r26_8017D648(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_r26_8017D650(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32 func_neo_ark_r26_8017D694(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_r26_8017D69C(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_neo_ark_r26_8017E19C[1];
extern GpRoomBoundVec D_neo_ark_r26_8017E9EC[5];
extern GpRoomCoordSet D_neo_ark_r26_8017E928[1];

extern AnimationPlayRequest D_neo_ark_r26_8017D780;
extern AnimationPlayRequest D_neo_ark_r26_8017D7C4;
extern AnimationPlayRequest D_neo_ark_r26_8017D7D8;
extern AnimationPlayRequest D_neo_ark_r26_8017D864;
extern AnimationPlayRequest D_neo_ark_r26_8017D904;
extern AnimationPlayRequest D_neo_ark_r26_8017D918;
extern AnimationPlayRequest D_neo_ark_r26_8017D9CC;
extern GpCmdArg             D_neo_ark_r26_8017D798;
void                        func_neo_ark_r26_8017D5D0(void);

AnimationPlayRequest D_neo_ark_r26_8017D780 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpCmdArg D_neo_ark_r26_8017D794 = { { .loc = { 4, 36 } }, 0 };

GpCmdArg D_neo_ark_r26_8017D798 = { { .loc = { 4, 36 } }, 1 };

AnimationPlayRequest D_neo_ark_r26_8017D79C[2] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 2, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_neo_ark_r26_8017D7C4 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017D7D8 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017D7EC[6] = {
    { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_neo_ark_r26_8017D864 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017D878[7] = {
    { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_neo_ark_r26_8017D904 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017D918 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017D92C[8] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_neo_ark_r26_8017D9CC = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_neo_ark_r26_8017D9E0 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017D9F4 = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017DA08 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017DA1C = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_r26_8017DA30 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpXformArg D_neo_ark_r26_8017DA44 = { { 3270, -0x2710, -1630, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_neo_ark_r26_8017DA5C = { { 0, 0, 850, 0 }, { 0, 0, 0, 0 } };

GpEvsCmd D_neo_ark_r26_8017DA74[57] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 5 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_r26_8017D780 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_neo_ark_r26_8017DA44 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2011 }, { .storage = &D_neo_ark_r26_8017D798 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_neo_ark_r26_8017DA5C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D7C4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D864 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D904 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017DA1C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D904 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017DA30 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D7D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D918 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D7D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D9F4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017DA08 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D7D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D7D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_neo_ark_r26_8017D9CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 5 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_neo_ark_r26_8017D5D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_neo_ark_r26_8017DFCC[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_neo_ark_r26_8017D5D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpMsgEntry D_neo_ark_r26_8017E0A4[5] = {
    { 5102, func_neo_ark_r26_8017D650 },
    { 5105, func_neo_ark_r26_8017D648 },
    { 5103, func_neo_ark_r26_8017D69C },
    { 5104, func_neo_ark_r26_8017D694 },
    { 0x7FFFFFFF, NULL },
};

GpRoomObjRec D_neo_ark_r26_8017E0CC[1] = {
    { D_neo_ark_r26_8017E19C, NULL, NULL, NULL },
};

GpRoomCoordRec D_neo_ark_r26_8017E0DC[1] = {
    { D_neo_ark_r26_8017E928, D_neo_ark_r26_8017E9EC },
};

u8* D_neo_ark_r26_8017E0E4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_r26_8017E0E8[1] = {
    { { .bytes = { 4, 0 } } },
};

GpWarpRec D_neo_ark_r26_8017E0EC[1] = {
    { { .words = { 2048, 0, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 2048, 0, 0, 0 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_neo_ark_r26_8017E124[1] = {
#include "assets/neo_ark_r26_collision_00BDC_normals.inc"
};

SVECTOR D_neo_ark_r26_8017E12C[4] = {
#include "assets/neo_ark_r26_collision_00BDC_verts.inc"
};

GpGridFace D_neo_ark_r26_8017E14C[1] = {
#include "assets/neo_ark_r26_collision_00BDC_faces.inc"
};

s16 D_neo_ark_r26_8017E158[16] = {
#include "assets/neo_ark_r26_collision_00BDC_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_r26_8017E158[i])
s16* D_neo_ark_r26_8017E178[9] = {
#include "assets/neo_ark_r26_collision_00BDC_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_r26_8017E19C[1] = {
    { NULL, D_neo_ark_r26_8017E124, D_neo_ark_r26_8017E12C, D_neo_ark_r26_8017E14C, D_neo_ark_r26_8017E178, 4000, 4000, 3, 3, 4000, 1 },
};

GpViewRec D_neo_ark_r26_8017E1C0[4] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x32C8, 0 } }, 257 },
    { { { { -3897, 0, -1261 }, { 0, 4096, 0 }, { 1261, 0, -3897 } }, { 1260, 1300, -5690 } }, 680 },
    { { { { 3982, 0, -955 }, { 108, 4069, 451 }, { 949, -463, 3957 } }, { 1470, 1070, 2380 } }, 257 },
    { { { { 3157, 0, 2608 }, { 2381, 1673, -2882 }, { -1065, 3738, 1289 } }, { -570, 2000, 760 } }, 257 },
};

GpSprtCmd D_neo_ark_r26_8017E250[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_r26_8017E260[62] = {
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 104, 16, 0, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, 24, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 40, 0, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 40, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 72, 0, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 64, 0, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 16, 0, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 16, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 16, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 32, 16, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 8, 16, 0, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 0, 16, 0, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 16, 0, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 16, 0, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 48, 0, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 48, 0, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 48, 0, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 48, 0, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, 48, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, 48, 0, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 16, 0, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -120, 16, 0, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -80, 16, 0, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 56, 1000, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 64, 1000, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 16, 1000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 24, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 24, 1000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 40, 1000, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 32, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 80, 64, 1000, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 80, 72, 1000, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, 80, 1000, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 72, 1000, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 72, 1000, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 64, 1000, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -48, 56, 1000, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 48, 0, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 48, 0, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 48, 0, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 48, 0, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 80, 1000, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 48, 0, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, 72, 1000, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 1000, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 40, 96, 1000, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 88, 1000, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 1000, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -160, 88, 1000, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -120, 72, 1000, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -96, 88, 1000, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, 88, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, 80, 1000, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 80, 0, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 80, 0, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -88, 112, 250, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -88, 96, 250, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -88, 80, 250, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -144, 80, 250, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -144, 96, 250, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, 112, 250, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_neo_ark_r26_8017E738[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 54, 0, 0, { 1, 0 } },
    { 54, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_r26_8017E758[14] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 0, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 0, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 8, 24, 0, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 32, 0, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 32, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 40, 0, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 96, 0, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -16, 24, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 8, 40, 1150, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, 32, 1375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 40, 1375, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 48, 1375, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, 32, 961, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, 48, 1131, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_neo_ark_r26_8017E870[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_r26_8017E888[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_r26_8017E898[4] = {
    { { .empty = D_neo_ark_r26_8017E250 }, D_neo_ark_r26_8017E250, NULL },
    { { .elements = D_neo_ark_r26_8017E260 }, D_neo_ark_r26_8017E738, NULL },
    { { .elements = D_neo_ark_r26_8017E758 }, D_neo_ark_r26_8017E870, NULL },
    { { .empty = D_neo_ark_r26_8017E888 }, D_neo_ark_r26_8017E888, NULL },
};

GpPointLight D_neo_ark_r26_8017E8C8[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2500, 3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2458, 1640, 820, { 0, 0 } }, 7300, 9300 },
};

GpRoomCoordSet D_neo_ark_r26_8017E928[1] = {
    { 0, NULL, 1, D_neo_ark_r26_8017E8C8, 0, NULL },
};

GpAreaTmdRec D_neo_ark_r26_8017E940[3] = {
    { 111, 439, 0, 0, { 0, 0 }, D_801413EC },
    { 112, 601, 5, 0, { 0, 0 }, D_80149664 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_neo_ark_r26_8017E964[3] = {
    { 111, 0, 0, 0, 0, 2850, 2048, 0, 0, 2, 0 },
    { 112, 0, 0, 0, 0, 855, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_neo_ark_r26_8017E994[11] = {
    { NULL, NULL },
    { D_neo_ark_r26_8017E964, D_neo_ark_r26_8017E940 },
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

GpRoomBoundVec D_neo_ark_r26_8017E9EC[5] = {
    { 4, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 1000, 700, 800, 825 },
    { 1000, 700, 800, 825 },
    { 16, 16, 16, 16 },
};

s32 D_neo_ark_r26_8017EA14[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

GpRoomParamRec D_neo_ark_r26_8017EA20[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_r26_8017EA28[1] = {
    { 0, 0, 1, 0, D_neo_ark_r26_8017EA14 },
};

GpRoomParamRec* D_neo_ark_r26_8017EA30[8] = {
    D_neo_ark_r26_8017EA20,
    D_neo_ark_r26_8017EA28,
    D_neo_ark_r26_8017EA20,
    D_neo_ark_r26_8017EA20,
    D_neo_ark_r26_8017EA20,
    D_neo_ark_r26_8017EA20,
    D_neo_ark_r26_8017EA20,
    D_neo_ark_r26_8017EA20,
};

static void func_neo_ark_r26_8017D6A4(Task* arg0);
static void func_neo_ark_r26_8017D710(Task* task);

/// Script callback: unless attract demo 9 is playing, points the save's
/// location at stage 5, area 0x1C, warp 1, room 1, sets `gDisplayState.spriteVariant`, spawns
/// task 0x11 and starts loading that location.
void func_neo_ark_r26_8017D5D0(void)
{
    if (Mc_SaveData[0].state.demoScene != 9) {
        Mc_SaveData[0].state.at4.loc.stage = 5;
        Mc_SaveData[0].state.at4.loc.area  = 0x1C;
        Mc_SaveData[0].state.at4.loc.warp  = 1;
        Mc_SaveData[0].state.at4.loc.room  = 1;
        gDisplayState.spriteVariant        = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad((u8*)&Mc_SaveData[0].state.at4.loc, 1);
    }
}

s32 func_neo_ark_r26_8017D648(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler for the save location: copies the incoming `GpSaveLoc`
/// onto the outgoing one and passes both to `func_map_neo_ark_80179B14`. Returns 1.
s32 func_neo_ark_r26_8017D650(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_r26_8017D694(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_r26_8017D69C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room task state 0: installs the message table, claims pointer slot 7, then
/// starts the room script unless the attract demo 9 is playing. Advances to
/// state 1.
static void func_neo_ark_r26_8017D6A4(Task* arg0)
{
    arg0->msgTable = D_neo_ark_r26_8017E0A4;
    Game_SetPtrSlot(arg0, 7);
    if (Mc_SaveData[0].state.demoScene != 9) {
        func_800E8634(D_neo_ark_r26_8017DA74, 0, D_neo_ark_r26_8017DFCC);
    }
    arg0->state = arg0->state + 1;
}

/// Room task state 1: does nothing, keeping the task alive.
static void func_neo_ark_r26_8017D710(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room task `func_neo_ark_r26_8017D720`, indexed by
/// `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_neo_ark_r26_8017D5C4 = {
    {
        func_neo_ark_r26_8017D6A4,
        func_neo_ark_r26_8017D710,
        taskKill,
    },
};

/// Room task: dispatches through a stack copy of its state table.
void func_neo_ark_r26_8017D720(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_r26_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_r26_8017D778(Task* unused)
{
}
