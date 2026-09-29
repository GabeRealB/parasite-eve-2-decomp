#include "rooms/acropolis_cafeteria.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "acropolis_cafeteria_private.h"

#include "actors/actor_202900.h"

#include "actors/actor_210600.h"

#include "actors/actor_310600.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

extern GpAnimSet* D_acropolis_cafeteria_80182C60[4];

extern GpAnimSet* D_acropolis_cafeteria_80182C40[1];

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

extern GpMsgEntry D_acropolis_cafeteria_80182AA8[];

extern GpXformArg D_acropolis_cafeteria_80182D28;
extern s32        D_acropolis_cafeteria_80182DB8;
extern GpXformArg D_acropolis_cafeteria_80182DDC;
extern GpEvsCmd   D_acropolis_cafeteria_80182E74[];
extern GpEvsCmd   D_acropolis_cafeteria_801831BC[];
extern GpEvsCmd   D_acropolis_cafeteria_8018330C[];
extern GpEvsCmd   D_acropolis_cafeteria_801834D4[];
extern GpEvsCmd   D_acropolis_cafeteria_8018363C[];
extern GpEvsCmd   D_acropolis_cafeteria_80183DBC[];
extern GpEvsCmd   D_acropolis_cafeteria_80183F3C[];
extern s32        D_acropolis_cafeteria_80184164;
extern RECT       D_acropolis_cafeteria_80184168;
extern RECT       D_acropolis_cafeteria_80184170;

static void func_acropolis_cafeteria_8017D6AC(Task* task);
static void func_acropolis_cafeteria_8017E348(Task* task);

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_cafeteria_8017D5C4 = {
    { func_acropolis_cafeteria_8017E348, func_acropolis_cafeteria_8017D6AC, taskKill },
};

static const char CafeteriaPlayerLabel[12] = "Player";

extern GpGridParams D_acropolis_cafeteria_801887A8[1];
extern GpObj3A      D_acropolis_cafeteria_80189C94[2];
extern GpObj4C      D_acropolis_cafeteria_801887CC[16];
extern GpObj4C      D_acropolis_cafeteria_80188C8C[18];
extern GpObj4C      D_acropolis_cafeteria_801891E4[16];
extern GpObj4C      D_acropolis_cafeteria_801896A4[20];

extern GpAnimSet D_acropolis_cafeteria_80184CC4;
s32              func_acropolis_cafeteria_8017D700(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32              func_acropolis_cafeteria_8017E0D4(Task*, s32, GpMessageArg, GpMessageArg);
s32              func_acropolis_cafeteria_8017E0DC(Task*, s32, s32, s32);
s32              func_acropolis_cafeteria_8017E154(Task*, s32, GpMsg13EF*, s32);
s32              func_acropolis_cafeteria_8017E22C(Task*, s32, s32, s32);
void             func_acropolis_cafeteria_8017D8F8(Task*);
void             func_acropolis_cafeteria_8017DD1C(Task*);
void             func_acropolis_cafeteria_8017DF68(Task*);
void             func_acropolis_cafeteria_8017E27C(s32);
void             func_acropolis_cafeteria_8017E2B0(void);
void             func_acropolis_cafeteria_8017E2D0(void);
void             func_acropolis_cafeteria_8017E310(void);

GpMsgEntry D_acropolis_cafeteria_80182AA8[6] = {
    { 5102, func_acropolis_cafeteria_8017D700 },
    { 5103, func_acropolis_cafeteria_8017E154 },
    { 5104, func_acropolis_cafeteria_8017E0DC },
    { 5105, func_acropolis_cafeteria_8017E0D4 },
    { 5106, func_acropolis_cafeteria_8017E22C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_acropolis_cafeteria_80182AD8[4] = {
    { 0, 32, func_acropolis_cafeteria_8017D8F8, { .model = NULL } },
    { 0, 32, func_acropolis_cafeteria_8017DD1C, { .model = NULL } },

    { 0, 32, func_acropolis_cafeteria_8017DF68, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpAnimArg D_acropolis_cafeteria_80182B08 = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182B1C = { { .index = 1 }, 1, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182B30 = { { .index = 1 }, 8, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182B44 = { { .index = 1 }, 9, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182B58 = { { .index = 0 }, 1, 0, 0, 0 };

GpXformArg D_acropolis_cafeteria_80182B6C = { { 0, 128, 0, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182B84 = { { -3759, -300, 18, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182B9C = { { -3748, -300, -885, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182BB4 = { { -3689, -300, -2934, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182BCC = { { -3892, -300, -1836, 0 }, { 0, 1800, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182BE4 = { { -3000, -300, 1000, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182BFC = { { -3000, -300, -600, 0 }, { 0, 2304, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182C14 = { { -2800, -300, -892, 0 }, { 0, 2048, 0, 0 } };

GpAnimArg D_acropolis_cafeteria_80182C2C = { { .index = 0 }, 0, 0, 0, 0 };

GpAnimSet* D_acropolis_cafeteria_80182C40[1] = {
    &D_actor_202900_8014FEB8,
};

GpCopyArg D_acropolis_cafeteria_80182C44 = { { .sets = D_acropolis_cafeteria_80182C40 }, 1 };

GpAnimArg D_acropolis_cafeteria_80182C4C = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimSet* D_acropolis_cafeteria_80182C60[4] = {
    &D_actor_310600_801668FC,
    &D_acropolis_cafeteria_80184CC4,
    &D_actor_210600_80151D14,
    &D_actor_210600_80153E70,
};

GpCopyArg D_acropolis_cafeteria_80182C70 = { { .sets = D_acropolis_cafeteria_80182C60 }, 4 };

GpAnimArg D_acropolis_cafeteria_80182C78 = { { .index = 1 }, 47, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182C8C = { { .index = 1 }, 48, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182CA0 = { { .index = 1 }, 49, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182CB4 = { { .index = 1 }, 50, 0, 0, 0 };

GpXformArg D_acropolis_cafeteria_80182CC8 = { { -3106, -300, -2056, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182CE0 = { { -3860, -299, -1365, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182CF8 = { { -3890, -299, -1555, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182D10 = { { -3490, -299, -4500, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182D28 = { { -3860, -299, -1253, 0 }, { 0, 200, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182D40 = { { -3290, -299, -3500, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182D58 = { { -3650, -299, -961, 0 }, { 0, 200, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182D70 = { { -3026, -300, -4126, 0 }, { 0, -200, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182D88 = { { -470, -1400, 701, 0 }, { 0, -800, 0, 0 } };

GpCmdArg D_acropolis_cafeteria_80182DA0 = { { .loc = { 1, 4 } }, 0 };

GpCmdArg D_acropolis_cafeteria_80182DA4 = { { .loc = { 1, 4 } }, 1 };

GpCmdArg D_acropolis_cafeteria_80182DA8 = { { .loc = { 1, 4 } }, 2 };

GpCmdArg D_acropolis_cafeteria_80182DAC = { { .loc = { 1, 4 } }, 3 };

GpCmdArg D_acropolis_cafeteria_80182DB0 = { { .loc = { 1, 4 } }, 4 };

GpCmdArg D_acropolis_cafeteria_80182DB4 = { { .loc = { 1, 4 } }, 5 };

s32 D_acropolis_cafeteria_80182DB8 = 0x70401;

GpCmdArg D_acropolis_cafeteria_80182DBC = { { .loc = { 1, 4 } }, 8 };

GpCmdArg D_acropolis_cafeteria_80182DC0 = { { .loc = { 1, 4 } }, 9 };

GpXformArg D_acropolis_cafeteria_80182DC4 = { { -3403, -250, 800, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182DDC = { { -3850, -299, -900, 0 }, { 0, 2048, 0, 0 } };

GpXformArg D_acropolis_cafeteria_80182DF4 = { { -4400, -299, -1400, 0 }, { 0, 2048, 0, 0 } };

GpAnimArg D_acropolis_cafeteria_80182E0C = { { .index = 0 }, 1, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182E20 = { { .index = 0 }, 2, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182E34 = { { .index = 0 }, 3, 0, 0, 0 };

GpAnimArg D_acropolis_cafeteria_80182E48 = { { .index = 0 }, 4, 0, 0, 0 };

GpOverlayIds D_acropolis_cafeteria_80182E5C = { 1, 4, 11 };

GpOverlayIds D_acropolis_cafeteria_80182E64 = { 1, 6, 11 };

GpOverrideArg D_acropolis_cafeteria_80182E6C = { 19, 1 };

GpEvsCmd D_acropolis_cafeteria_80182E74[35] = {
    { 13, { .callbackNoArg = func_acropolis_cafeteria_8017E2B0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_acropolis_cafeteria_80182C44 }, { .value = 0 } },
    { 26, { .value = 76 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_cafeteria_80182B58 }, { .value = 0 } },
    { 12, { .overlays = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182C4C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1020 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182B6C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_cafeteria_8017E27C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 69 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 20, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 21, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 37, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182B84 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_801831BC[14] = {
    { 20, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 21, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 4, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182B84 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_8018330C[19] = {
    { 13, { .callbackNoArg = func_acropolis_cafeteria_8017E310 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B44 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_cafeteria_8017E2D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182CE0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DA8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5104000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 320 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B30 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_801834D4[15] = {
    { 19, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x5104000C }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DB0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_cafeteria_8017E2D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182CE0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B30 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_8018363C[80] = {
    { 12, { .overlays = &D_acropolis_cafeteria_80182E64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 32, { .value = 100 }, { .value = 100 }, { .value = 70 }, { .value = 0 }, { .value = 0 } },
    { 40, { .value = 128 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182CF8 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_acropolis_cafeteria_80182C70 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182B9C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182C78 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DC0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DB0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182BB4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D40 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DB4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182DC4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_acropolis_cafeteria_80182E20 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D10 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DAC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182C8C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D28 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182DDC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DC0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_acropolis_cafeteria_80182E0C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182BE4 }, { .value = 0 } },
    { 4, { .value = 76 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_acropolis_cafeteria_80182BFC }, { .storage = &D_acropolis_cafeteria_80182E6C } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 57 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_acropolis_cafeteria_80182BFC }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D58 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DBC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182DDC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_acropolis_cafeteria_80182E34 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182DDC }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182CA0 }, { .value = 0 } },
    { 4, { .value = 686 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D88 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182DF4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_acropolis_cafeteria_80182E48 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182DF4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182CB4 }, { .value = 0 } },
    { 4, { .value = 252 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 40, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_80183DBC[10] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 40, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_80183EAC[6] = {
    { 22, { .value = 4 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 4, { .value = 250 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_80183F3C[19] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182CC8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DA4 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_cafeteria_80182BCC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D70 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_cafeteria_80184104[4] = {
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_cafeteria_80182D88 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2011 }, { .storage = &D_acropolis_cafeteria_80182DA4 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_acropolis_cafeteria_80184164 = 0;

RECT D_acropolis_cafeteria_80184168 = { 704, 0, 64, 256 };

RECT D_acropolis_cafeteria_80184170 = { 0, 271, 256, 1 };

TaskDesc D_acropolis_cafeteria_80184178[3] = {
    { 0, 192, func_acropolis_cafeteria_8017E6B8, { .model = NULL } },
    { 0, 192, func_acropolis_cafeteria_8017E658, { .model = NULL } },
    { 0, 192, func_acropolis_cafeteria_8017E47C, { .model = NULL } },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[23];
    GpPackedSvec words[69];
} AcropolisCafeteriaPoseBank6BDC;

AcropolisCafeteriaPoseBank6BDC D_acropolis_cafeteria_8018419C = { .poses = {
#include "assets/acropolis_cafeteria_animation_07704_bank1.inc"
                                                                  } };

GpPackedSvec D_acropolis_cafeteria_801842B0[286] = {
#include "assets/acropolis_cafeteria_animation_07704_bank4.inc"
};

GpAnimRec D_acropolis_cafeteria_80184728[349] = {
#include "assets/acropolis_cafeteria_animation_07704_records.inc"
};

u16 D_acropolis_cafeteria_80184C9C[20] = {
#include "assets/acropolis_cafeteria_animation_07704_indices.inc"
};

GpAnimSet D_acropolis_cafeteria_80184CC4 = {
    D_acropolis_cafeteria_80184728,
    D_acropolis_cafeteria_80184C9C,
    { NULL, D_acropolis_cafeteria_8018419C.words, NULL, NULL, D_acropolis_cafeteria_801842B0, NULL, NULL, NULL },
};

GpMsgEntry D_acropolis_cafeteria_80184CEC[2] = {
    { 3000, func_acropolis_cafeteria_8017F908 },
    { 0x7FFFFFFF, NULL },
};

s32 D_acropolis_cafeteria_80184CFC = 0;

TmdBone D_acropolis_cafeteria_80184D00[1] = {
#include "assets/acropolis_cafeteria_model_0789C_skeleton.inc"
};

u32 D_acropolis_cafeteria_80184D24[1] = {
#include "assets/acropolis_cafeteria_model_0789C_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_80184D28[14] = {
#include "assets/acropolis_cafeteria_model_0789C_verts.inc"
};

u32 D_acropolis_cafeteria_80184D98[49] = {
#include "assets/acropolis_cafeteria_model_0789C_stream.inc"
};

TmdSource D_acropolis_cafeteria_80184E5C = {
    0,
    296,
    0,
    1,
    D_acropolis_cafeteria_80184D24,
    D_acropolis_cafeteria_80184D28,
    &D_acropolis_cafeteria_80184D28[14],
    D_acropolis_cafeteria_80184D00,
    D_acropolis_cafeteria_80184D98,
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_acropolis_cafeteria_80184E80[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

TmdBone D_acropolis_cafeteria_80184E90[1] = {
#include "assets/acropolis_cafeteria_model_08304_skeleton.inc"
};

u32 D_acropolis_cafeteria_80184EB4[1] = {
#include "assets/acropolis_cafeteria_model_08304_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_80184EB8[46] = {
#include "assets/acropolis_cafeteria_model_08304_verts.inc"
};

SVECTOR D_acropolis_cafeteria_80185028[72] = {
#include "assets/acropolis_cafeteria_model_08304_normals.inc"
};

u32 D_acropolis_cafeteria_80185268[407] = {
#include "assets/acropolis_cafeteria_model_08304_stream.inc"
};

TmdSource D_acropolis_cafeteria_801858C4 = {
    0,
    2952,
    0,
    1,
    D_acropolis_cafeteria_80184EB4,
    D_acropolis_cafeteria_80184EB8,
    D_acropolis_cafeteria_80185028,
    D_acropolis_cafeteria_80184E90,
    D_acropolis_cafeteria_80185268,
};

TmdBone D_acropolis_cafeteria_801858E8[1] = {
#include "assets/acropolis_cafeteria_model_08C9C_skeleton.inc"
};

u32 D_acropolis_cafeteria_8018590C[1] = {
#include "assets/acropolis_cafeteria_model_08C9C_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_80185910[31] = {
#include "assets/acropolis_cafeteria_model_08C9C_verts.inc"
};

SVECTOR D_acropolis_cafeteria_80185A08[62] = {
#include "assets/acropolis_cafeteria_model_08C9C_normals.inc"
};

u32 D_acropolis_cafeteria_80185BF8[409] = {
#include "assets/acropolis_cafeteria_model_08C9C_stream.inc"
};

TmdSource D_acropolis_cafeteria_8018625C = {
    0,
    2880,
    0,
    1,
    D_acropolis_cafeteria_8018590C,
    D_acropolis_cafeteria_80185910,
    D_acropolis_cafeteria_80185A08,
    D_acropolis_cafeteria_801858E8,
    D_acropolis_cafeteria_80185BF8,
};

TmdBone D_acropolis_cafeteria_80186280[1] = {
#include "assets/acropolis_cafeteria_model_096EC_skeleton.inc"
};

u32 D_acropolis_cafeteria_801862A4[1] = {
#include "assets/acropolis_cafeteria_model_096EC_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_801862A8[46] = {
#include "assets/acropolis_cafeteria_model_096EC_verts.inc"
};

SVECTOR D_acropolis_cafeteria_80186418[71] = {
#include "assets/acropolis_cafeteria_model_096EC_normals.inc"
};

u32 D_acropolis_cafeteria_80186650[407] = {
#include "assets/acropolis_cafeteria_model_096EC_stream.inc"
};

TmdSource D_acropolis_cafeteria_80186CAC = {
    0,
    2952,
    0,
    1,
    D_acropolis_cafeteria_801862A4,
    D_acropolis_cafeteria_801862A8,
    D_acropolis_cafeteria_80186418,
    D_acropolis_cafeteria_80186280,
    D_acropolis_cafeteria_80186650,
};

TmdBone D_acropolis_cafeteria_80186CD0[1] = {
#include "assets/acropolis_cafeteria_model_09F58_skeleton.inc"
};

u32 D_acropolis_cafeteria_80186CF4[1] = {
#include "assets/acropolis_cafeteria_model_09F58_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_80186CF8[37] = {
#include "assets/acropolis_cafeteria_model_09F58_verts.inc"
};

SVECTOR D_acropolis_cafeteria_80186E20[56] = {
#include "assets/acropolis_cafeteria_model_09F58_normals.inc"
};

u32 D_acropolis_cafeteria_80186FE0[334] = {
#include "assets/acropolis_cafeteria_model_09F58_stream.inc"
};

TmdSource D_acropolis_cafeteria_80187518 = {
    0,
    2296,
    0,
    1,
    D_acropolis_cafeteria_80186CF4,
    D_acropolis_cafeteria_80186CF8,
    D_acropolis_cafeteria_80186E20,
    D_acropolis_cafeteria_80186CD0,
    D_acropolis_cafeteria_80186FE0,
};

GpRoomObjRec D_acropolis_cafeteria_8018753C[4] = {
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_801887CC, D_acropolis_cafeteria_801891E4, D_acropolis_cafeteria_80189C94 },
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_801887CC, D_acropolis_cafeteria_801896A4, D_acropolis_cafeteria_80189C94 },
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_801887CC, D_acropolis_cafeteria_801896A4, D_acropolis_cafeteria_80189C94 },
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_80188C8C, D_acropolis_cafeteria_801896A4, D_acropolis_cafeteria_80189C94 },
};

u8 D_acropolis_cafeteria_8018757C[24] = {
    1,
    2,
    21,
    14,
    5,
    6,
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
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_acropolis_cafeteria_80187594[24] = {
    1,
    2,
    21,
    22,
    15,
    6,
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
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8* D_acropolis_cafeteria_801875AC[4] = {
    D_8010CAF8,
    D_acropolis_cafeteria_8018757C,
    D_acropolis_cafeteria_8018757C,
    D_acropolis_cafeteria_80187594,
};

GpViewCountRec D_acropolis_cafeteria_801875BC[4] = {
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
};

GpRoomCoordRec D_acropolis_cafeteria_801875C4[4] = {
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
};

GpWarpRec D_acropolis_cafeteria_801875E4[3] = {
    { { .words = { 0, 1841, -236, -3256 } }, { 0, 0, 0, 0 }, { .words = { 0, 1841, -236, -3256 } }, { 0, 0, 0, 0 }, 0x51040009, 0x51040008, 0, 7, 0, 499 },
    { { .words = { 2048, -4047, -236, 2765 } }, { 0, 0, 0, 0 }, { .words = { 2048, -4047, -236, 2765 } }, { 0, 0, 0, 0 }, 0x51040002, 0x51040001, 0, 2, 0, 501 },
    { { .words = { 3072, -986, -300, -5486 } }, { 0, 0, 0, 0 }, { .words = { 3072, -986, -300, -5486 } }, { 0, 0, 0, 0 }, 0x51040005, 0x51040004, 0x51040003, 5, 0, 500 },
};

SVECTOR D_acropolis_cafeteria_8018768C[36] = {
    { 0, 4096, 0, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 0, -4096, 0, 0 },
    { 4096, 0, 0, 0 },
    { -6, 0, 4096, 0 },
    { -3095, 0, 2683, 0 },
    { -3316, 0, -2405, 0 },
    { -3204, 0, 2551, 0 },
    { 3485, 0, 2153, 0 },
    { -2842, 0, 2950, 0 },
    { 1053, 0, 3958, 0 },
    { 3544, 0, -2053, 0 },
    { -3511, 0, 2110, 0 },
    { -2695, 0, -3085, 0 },
    { 3705, 0, -1747, 0 },
    { 3913, 0, 1211, 0 },
    { -4096, 0, 12, 0 },
    { 1210, 0, -3913, 0 },
    { 3656, 0, -1847, 0 },
    { 1421, 0, 3842, 0 },
    { -3801, 0, -1526, 0 },
    { 1369, 0, -3860, 0 },
    { 4096, 0, 1, 0 },
    { 3295, 0, 2432, 0 },
    { -4096, 0, 1, 0 },
    { 0, 0, -4096, 0 },
    { 873, 0, -4002, 0 },
    { 2904, 0, 2889, 0 },
    { -51, 0, 4096, 0 },
    { -3862, 0, 1364, 0 },
    { -2464, 0, -3272, 0 },
    { 3788, 0, -1558, 0 },
    { 14, 0, -4096, 0 },
    { -4096, 0, 6, 0 },
};

SVECTOR D_acropolis_cafeteria_801877AC[207] = {
    { 3501, -2800, 3004, 0 },
    { -279, -2800, 3004, 0 },
    { -279, -2800, -3496, 0 },
    { 3501, -2800, -3496, 0 },
    { -279, -300, -3496, 0 },
    { 3501, -300, -3496, 0 },
    { 3501, -300, 3004, 0 },
    { -279, -300, 3004, 0 },
    { -221, -2103, -2155, 0 },
    { -221, -2447, -2155, 0 },
    { -221, -2447, -2980, 0 },
    { -221, -2103, -2980, 0 },
    { -335, -300, 2193, 0 },
    { -335, -2301, 2193, 0 },
    { 2064, -2301, 2193, 0 },
    { 2064, -300, 2193, 0 },
    { 2064, -2301, 3004, 0 },
    { 2064, -300, 3004, 0 },
    { -319, -2301, 3004, 0 },
    { 2064, -300, 3004, 0 },
    { 2064, -1897, 3004, 0 },
    { 2064, -1897, 2193, 0 },
    { 2064, -300, 2193, 0 },
    { 3027, -1897, 2193, 0 },
    { 3027, -300, 2193, 0 },
    { 3027, -1897, 3004, 0 },
    { 3027, -300, 3004, 0 },
    { 2775, -300, -1602, 0 },
    { 2775, -2100, -1602, 0 },
    { 2775, -2100, -3489, 0 },
    { 2775, -300, -3489, 0 },
    { 3501, -2100, -3489, 0 },
    { 3501, -300, -3489, 0 },
    { 3501, -2100, -1601, 0 },
    { 3501, -300, -1601, 0 },
    { 2775, -300, 1268, 0 },
    { 2775, -1147, 1268, 0 },
    { 2775, -1147, 36, 0 },
    { 2775, -300, 36, 0 },
    { 3501, -1147, 36, 0 },
    { 3501, -300, 36, 0 },
    { 3501, -1147, 2105, 0 },
    { 3501, -300, 2105, 0 },
    { 2775, -300, 37, 0 },
    { 2775, -1847, 37, 0 },
    { 2775, -1847, -785, 0 },
    { 2775, -300, -785, 0 },
    { 3501, -1847, -785, 0 },
    { 3501, -300, -785, 0 },
    { 3501, -1847, 37, 0 },
    { 3501, -300, 37, 0 },
    { 2632, -300, -780, 0 },
    { 2632, -1349, -780, 0 },
    { 2632, -1349, -1601, 0 },
    { 2632, -300, -1601, 0 },
    { 3501, -1349, -2799, 0 },
    { 3501, -300, -2799, 0 },
    { 3501, -1349, 312, 0 },
    { 3501, -300, 312, 0 },
    { 348, -300, -2956, 0 },
    { 348, -2201, -2956, 0 },
    { 348, -2201, -3496, 0 },
    { 348, -300, -3496, 0 },
    { 1217, -2201, -2956, 0 },
    { 1217, -2201, -3496, 0 },
    { 1217, -300, -3496, 0 },
    { 1217, -300, -2956, 0 },
    { -279, -2100, -2099, 0 },
    { 343, -2100, -2099, 0 },
    { 343, -2100, -3007, 0 },
    { -279, -2100, -3007, 0 },
    { -279, -300, -3007, 0 },
    { 343, -300, -3007, 0 },
    { 343, -300, -2099, 0 },
    { -279, -300, -2099, 0 },
    { -4518, -1205, -137, 0 },
    { -4496, -1205, -1396, 0 },
    { -5499, -1205, -1706, 0 },
    { -5493, -1205, 223, 0 },
    { -4518, -299, -137, 0 },
    { -4156, -299, -723, 0 },
    { -4156, -1205, -723, 0 },
    { -2608, -1205, -1758, 0 },
    { -2608, -299, -1758, 0 },
    { -2389, -299, -1547, 0 },
    { -2389, -1205, -1547, 0 },
    { 803, -300, -995, 0 },
    { 803, -769, -995, 0 },
    { -392, -769, -676, 0 },
    { -392, -300, -676, 0 },
    { 3105, -2202, 2992, 0 },
    { 3105, -2806, 2992, 0 },
    { 3105, -2806, -2673, 0 },
    { 3105, -2202, -2673, 0 },
    { 3501, -2806, -2673, 0 },
    { 3501, -2202, -2673, 0 },
    { 3501, -2202, 2992, 0 },
    { 3501, -2806, 2992, 0 },
    { 2779, -2107, -1560, 0 },
    { 2779, -2806, -1560, 0 },
    { 2779, -2806, -3495, 0 },
    { 2779, -2107, -3495, 0 },
    { 3501, -2806, -3495, 0 },
    { 3501, -2107, -3495, 0 },
    { 3501, -2107, -1560, 0 },
    { 3501, -2806, -1560, 0 },
    { -5499, -3300, 3004, 0 },
    { -5499, -300, 2307, 0 },
    { -5499, -300, -6000, 0 },
    { -5499, -3300, -6000, 0 },
    { -498, -3300, -6000, 0 },
    { -3526, -300, -6000, 0 },
    { -498, -300, -6000, 0 },
    { -498, -3300, 3004, 0 },
    { -4483, -300, 3004, 0 },
    { -5499, -300, 3004, 0 },
    { -3526, -300, 2307, 0 },
    { -498, -300, 2307, 0 },
    { -4483, -300, 2307, 0 },
    { -3526, -300, 3004, 0 },
    { -498, -300, 3004, 0 },
    { -4483, -300, -6000, 0 },
    { -499, -300, -1496, 0 },
    { -499, -3300, -1496, 0 },
    { -499, -3300, -6001, 0 },
    { -499, -300, -6001, 0 },
    { -279, -300, -1496, 0 },
    { -279, -3300, -1496, 0 },
    { -279, -300, -3501, 0 },
    { -279, -3300, -3501, 0 },
    { -1003, -300, 3003, 0 },
    { -1003, -1200, 3003, 0 },
    { -1003, -1200, -1499, 0 },
    { -1003, -300, -1499, 0 },
    { 21, -1200, -1499, 0 },
    { 21, -300, -1499, 0 },
    { 21, -1200, 3003, 0 },
    { 21, -300, 3003, 0 },
    { -5488, -300, 3005, 0 },
    { -5488, -1300, 3005, 0 },
    { -5488, -1300, 2409, 0 },
    { -5488, -300, 2409, 0 },
    { -4916, -1300, 2409, 0 },
    { -4916, -300, 2409, 0 },
    { -4570, -1300, 3005, 0 },
    { -4570, -300, 3005, 0 },
    { -498, -2300, 3003, 0 },
    { -498, -3300, 3003, 0 },
    { -498, -3300, -1501, 0 },
    { -498, -2300, -1501, 0 },
    { -400, -3300, -1501, 0 },
    { -400, -2300, -1501, 0 },
    { -400, -3300, 3003, 0 },
    { -400, -2300, 3003, 0 },
    { -2977, -299, 1407, 0 },
    { -2977, -1205, 1407, 0 },
    { -3407, -1205, 691, 0 },
    { -3407, -299, 691, 0 },
    { -2428, -1205, -165, 0 },
    { -2428, -299, -165, 0 },
    { -2000, -1205, 743, 0 },
    { -2000, -299, 743, 0 },
    { -2208, -1205, 1417, 0 },
    { -2208, -299, 1417, 0 },
    { -5493, -299, 223, 0 },
    { -5499, -299, -1706, 0 },
    { -4496, -299, -1396, 0 },
    { -2997, -299, -2860, 0 },
    { -2997, -1205, -2860, 0 },
    { -2870, -1205, -3175, 0 },
    { -2870, -299, -3175, 0 },
    { -1793, -1205, -2793, 0 },
    { -1793, -299, -2793, 0 },
    { -1793, -1205, -2353, 0 },
    { -1793, -299, -2353, 0 },
    { -5599, -299, -2869, 0 },
    { -5599, -1205, -2869, 0 },
    { -5600, -1205, -6098, 0 },
    { -5600, -299, -6098, 0 },
    { -2800, -1205, -6098, 0 },
    { -2800, -299, -6098, 0 },
    { -2460, -1205, -6024, 0 },
    { -2460, -299, -6024, 0 },
    { 2941, -299, 2314, 0 },
    { 2941, -1633, 2314, 0 },
    { 2941, -1633, 1290, 0 },
    { 2941, -299, 1290, 0 },
    { 3501, -1633, 1290, 0 },
    { 3501, -299, 1290, 0 },
    { 3501, -1633, 2314, 0 },
    { 3501, -299, 2314, 0 },
    { -1003, -301, -1499, 0 },
    { -1003, -1301, -1499, 0 },
    { -1003, -1301, -2602, 0 },
    { -1003, -301, -2602, 0 },
    { -499, -1301, -2982, 0 },
    { -499, -301, -2982, 0 },
    { -499, -1301, -1499, 0 },
    { -499, -301, -1499, 0 },
    { 178, -2103, -2155, 0 },
    { 178, -2447, -2155, 0 },
    { 178, -2103, -2980, 0 },
    { 178, -2447, -2980, 0 },
    { 351, -300, -2094, 0 },
    { 351, -769, -2094, 0 },
    { -394, -300, -2096, 0 },
    { -394, -769, -2096, 0 },
};

GpGridFace D_acropolis_cafeteria_80187E24[120] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 4, 5, 2, 3 }, 1, 0 },
    { { 5, 6, 3, 0 }, 2, 0 },
    { { 6, 7, 0, 1 }, 3, 0 },
    { { 6, 5, 7, 4 }, 4, 3 },
    { { 9, 10, 8, 11 }, 2, 0 },
    { { 13, 14, 12, 15 }, 3, 0 },
    { { 14, 16, 15, 17 }, 5, 0 },
    { { 16, 14, 18, 13 }, 4, 0 },
    { { 20, 21, 19, 22 }, 2, 0 },
    { { 21, 23, 22, 24 }, 3, 0 },
    { { 23, 25, 24, 26 }, 5, 0 },
    { { 25, 23, 20, 21 }, 4, 0 },
    { { 28, 29, 27, 30 }, 2, 0 },
    { { 29, 31, 30, 32 }, 3, 0 },
    { { 33, 31, 28, 29 }, 4, 0 },
    { { 33, 28, 34, 27 }, 6, 0 },
    { { 36, 37, 35, 38 }, 2, 0 },
    { { 37, 39, 38, 40 }, 3, 0 },
    { { 41, 39, 36, 37 }, 4, 0 },
    { { 41, 36, 42, 35 }, 7, 0 },
    { { 44, 45, 43, 46 }, 2, 0 },
    { { 45, 47, 46, 48 }, 3, 0 },
    { { 49, 47, 44, 45 }, 4, 0 },
    { { 49, 44, 50, 43 }, 1, 0 },
    { { 52, 53, 51, 54 }, 2, 0 },
    { { 53, 55, 54, 56 }, 8, 0 },
    { { 57, 55, 52, 53 }, 4, 0 },
    { { 57, 52, 58, 51 }, 9, 0 },
    { { 60, 61, 59, 62 }, 2, 0 },
    { { 63, 64, 60, 61 }, 4, 0 },
    { { 64, 63, 65, 66 }, 5, 0 },
    { { 63, 60, 66, 59 }, 1, 0 },
    { { 68, 69, 67, 70 }, 4, 0 },
    { { 70, 69, 71, 72 }, 3, 0 },
    { { 69, 68, 72, 73 }, 5, 0 },
    { { 68, 67, 73, 74 }, 1, 0 },
    { { 76, 77, 75, 78 }, 4, 0 },
    { { 79, 80, 75, 81 }, 10, 0 },
    { { 83, 84, 82, 85 }, 11, 0 },
    { { 87, 88, 86, 89 }, 12, 4 },
    { { 91, 92, 90, 93 }, 2, 0 },
    { { 92, 94, 93, 95 }, 3, 0 },
    { { 90, 93, 96, 95 }, 0, 0 },
    { { 97, 91, 96, 90 }, 1, 0 },
    { { 99, 100, 98, 101 }, 2, 0 },
    { { 100, 102, 101, 103 }, 3, 0 },
    { { 98, 101, 104, 103 }, 0, 0 },
    { { 105, 99, 104, 98 }, 1, 0 },
    { { 107, 108, 106, 109 }, 5, 0 },
    { { 110, 111, 112, 0xFFFF }, 1, 0 },
    { { 106, 109, 113, 110 }, 0, 0 },
    { { 106, 114, 115, 0xFFFF }, 3, 0 },
    { { 116, 117, 111, 112 }, 4, 2 },
    { { 118, 107, 114, 115 }, 4, 2 },
    { { 119, 120, 116, 117 }, 4, 2 },
    { { 121, 108, 118, 107 }, 4, 2 },
    { { 111, 121, 116, 118 }, 4, 2 },
    { { 116, 118, 119, 114 }, 4, 1 },
    { { 121, 109, 108, 0xFFFF }, 1, 0 },
    { { 121, 111, 109, 110 }, 1, 0 },
    { { 119, 113, 120, 0xFFFF }, 3, 0 },
    { { 106, 113, 114, 119 }, 3, 0 },
    { { 107, 106, 115, 0xFFFF }, 5, 0 },
    { { 123, 124, 122, 125 }, 2, 0 },
    { { 127, 123, 126, 122 }, 1, 0 },
    { { 129, 127, 128, 126 }, 5, 0 },
    { { 131, 132, 130, 133 }, 2, 0 },
    { { 132, 134, 133, 135 }, 3, 0 },
    { { 134, 136, 135, 137 }, 5, 0 },
    { { 136, 131, 137, 130 }, 1, 0 },
    { { 136, 134, 131, 132 }, 4, 0 },
    { { 139, 140, 138, 141 }, 2, 0 },
    { { 140, 142, 141, 143 }, 3, 0 },
    { { 142, 144, 143, 145 }, 13, 0 },
    { { 144, 142, 139, 140 }, 4, 0 },
    { { 147, 148, 146, 149 }, 2, 0 },
    { { 148, 150, 149, 151 }, 3, 0 },
    { { 150, 152, 151, 153 }, 5, 0 },
    { { 152, 147, 153, 146 }, 1, 0 },
    { { 146, 149, 153, 151 }, 0, 0 },
    { { 155, 156, 154, 157 }, 14, 0 },
    { { 156, 158, 157, 159 }, 15, 0 },
    { { 158, 160, 159, 161 }, 16, 0 },
    { { 160, 162, 161, 163 }, 17, 0 },
    { { 162, 156, 155, 0xFFFF }, 4, 0 },
    { { 78, 77, 164, 165 }, 18, 0 },
    { { 77, 76, 165, 166 }, 19, 0 },
    { { 76, 81, 166, 80 }, 20, 0 },
    { { 75, 78, 79, 164 }, 21, 0 },
    { { 76, 75, 81, 0xFFFF }, 4, 0 },
    { { 168, 169, 167, 170 }, 22, 0 },
    { { 169, 171, 170, 172 }, 23, 0 },
    { { 171, 173, 172, 174 }, 24, 0 },
    { { 173, 85, 174, 84 }, 25, 0 },
    { { 173, 171, 168, 169 }, 4, 0 },
    { { 176, 177, 175, 178 }, 26, 0 },
    { { 177, 179, 178, 180 }, 27, 0 },
    { { 179, 181, 180, 182 }, 28, 0 },
    { { 176, 175, 181, 182 }, 29, 0 },
    { { 162, 160, 156, 158 }, 4, 0 },
    { { 154, 163, 155, 162 }, 30, 0 },
    { { 176, 181, 177, 179 }, 4, 0 },
    { { 168, 82, 173, 85 }, 4, 0 },
    { { 82, 168, 83, 167 }, 31, 0 },
    { { 184, 185, 183, 186 }, 2, 0 },
    { { 185, 187, 186, 188 }, 3, 0 },
    { { 189, 187, 184, 185 }, 4, 0 },
    { { 189, 184, 190, 183 }, 1, 0 },
    { { 192, 193, 191, 194 }, 2, 0 },
    { { 193, 195, 194, 196 }, 32, 0 },
    { { 197, 195, 192, 193 }, 4, 0 },
    { { 197, 192, 198, 191 }, 1, 0 },
    { { 200, 9, 199, 8 }, 1, 0 },
    { { 202, 200, 201, 199 }, 5, 0 },
    { { 10, 202, 11, 201 }, 3, 0 },
    { { 204, 87, 203, 86 }, 33, 4 },
    { { 206, 204, 205, 203 }, 34, 0 },
    { { 88, 206, 89, 205 }, 35, 0 },
    { { 9, 200, 10, 202 }, 4, 0 },
};

s16 D_acropolis_cafeteria_801883C4[61] = {
    0,
    1,
    4,
    5,
    29,
    30,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    49,
    50,
    51,
    53,
    56,
    57,
    59,
    60,
    64,
    65,
    66,
    67,
    68,
    71,
    76,
    77,
    80,
    82,
    83,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    102,
    103,
    104,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    117,
    118,
    119,
    -1,
};

s16 D_acropolis_cafeteria_80188440[70] = {
    0,
    4,
    5,
    6,
    8,
    33,
    36,
    37,
    38,
    39,
    40,
    49,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
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
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    99,
    100,
    101,
    102,
    103,
    104,
    109,
    110,
    111,
    112,
    113,
    117,
    118,
    119,
    -1,
};

s16 D_acropolis_cafeteria_801884CC[39] = {
    0,
    3,
    4,
    6,
    8,
    37,
    49,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    61,
    62,
    63,
    67,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    89,
    100,
    101,
    -1,
};

s16 D_acropolis_cafeteria_8018851C[72] = {
    0,
    1,
    2,
    4,
    5,
    13,
    14,
    15,
    16,
    21,
    22,
    23,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    39,
    40,
    41,
    42,
    43,
    45,
    46,
    47,
    48,
    50,
    51,
    53,
    57,
    60,
    64,
    65,
    66,
    67,
    68,
    69,
    71,
    76,
    77,
    78,
    80,
    91,
    92,
    93,
    94,
    95,
    97,
    98,
    99,
    102,
    103,
    104,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    -1,
};

s16 D_acropolis_cafeteria_801885AC[93] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    15,
    16,
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
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    47,
    48,
    51,
    53,
    55,
    57,
    61,
    62,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    92,
    93,
    94,
    95,
    100,
    101,
    103,
    104,
    105,
    106,
    107,
    108,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    -1,
};

s16 D_acropolis_cafeteria_80188668[41] = {
    0,
    2,
    3,
    4,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    17,
    19,
    20,
    41,
    43,
    44,
    51,
    53,
    55,
    58,
    61,
    62,
    67,
    69,
    70,
    71,
    76,
    78,
    79,
    80,
    83,
    84,
    85,
    100,
    101,
    105,
    106,
    107,
    108,
    -1,
};

s16 D_acropolis_cafeteria_801886BC[27] = {
    0,
    1,
    2,
    4,
    13,
    14,
    15,
    16,
    21,
    22,
    23,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    41,
    42,
    43,
    45,
    46,
    47,
    48,
    -1,
};

s16 D_acropolis_cafeteria_801886F4[43] = {
    0,
    1,
    2,
    3,
    4,
    6,
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
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    105,
    106,
    107,
    108,
    116,
    -1,
};

s16 D_acropolis_cafeteria_8018874C[27] = {
    0,
    2,
    3,
    4,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    17,
    18,
    19,
    20,
    23,
    24,
    27,
    28,
    41,
    43,
    44,
    105,
    106,
    107,
    108,
    -1,
};

s16* D_acropolis_cafeteria_80188784[9] = {
    D_acropolis_cafeteria_801883C4,
    D_acropolis_cafeteria_80188440,
    D_acropolis_cafeteria_801884CC,
    D_acropolis_cafeteria_8018851C,
    D_acropolis_cafeteria_801885AC,
    D_acropolis_cafeteria_80188668,
    D_acropolis_cafeteria_801886BC,
    D_acropolis_cafeteria_801886F4,
    D_acropolis_cafeteria_8018874C,
};

GpGridParams D_acropolis_cafeteria_801887A8[1] = {
    { NULL, D_acropolis_cafeteria_8018768C, D_acropolis_cafeteria_801877AC, D_acropolis_cafeteria_80187E24, D_acropolis_cafeteria_80188784, 5600, 6098, 3, 3, 4000, 120 },
};

GpObj4C D_acropolis_cafeteria_801887CC[16] = {
    { NULL, NULL, NULL, { -1458, -1872, 1406, 0 }, { { -1092, 2672, -566, 0 }, { 1084, 2672, 560, 0 }, { -1092, -2672, -566, 0 }, { 1084, -2672, 560, 0 } }, { -1889, 0, 3647, 0 }, { 0, 0, 4096, 0 }, 2930, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -1538, 0, 1726, 0 }, { { 1286, 2672, 820, 0 }, { -1293, 2672, -825, 0 }, { 1286, -2672, 820, 0 }, { -1293, -2672, -825, 0 } }, { 2202, 0, -3454, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -4945, 0, -2273, 0 }, { { -1054, 2672, -199, 0 }, { 1047, 2672, 189, 0 }, { -1054, -2672, -199, 0 }, { 1047, -2672, 189, 0 } }, { -747, 0, 4036, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -4866, 0, -1921, 0 }, { { 1073, 2672, 165, 0 }, { -1077, 2672, -172, 0 }, { 1073, -2672, 165, 0 }, { -1077, -2672, -172, 0 } }, { 633, 0, -4048, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1442, 0, -4738, 0 }, { { 520, 2672, 1242, 0 }, { -521, 2672, -1243, 0 }, { 520, -2672, 1242, 0 }, { -521, -2672, -1243, 0 } }, { 3781, 0, -1586, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1313, 0, -5121, 0 }, { { -441, 2672, -1044, 0 }, { 434, 2672, 1037, 0 }, { -441, -2672, -1044, 0 }, { 434, -2672, 1037, 0 } }, { -3783, 0, 1588, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -817, 0, -3731, 0 }, { { 499, 2672, 345, 0 }, { -505, 2672, -353, 0 }, { 499, -2672, 345, 0 }, { -505, -2672, -353, 0 } }, { 2338, 0, -3366, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 1248, 0, 63, 0 }, { { 2033, 2672, -23, 0 }, { -2045, 2672, 15, 0 }, { 2033, -2672, -23, 0 }, { -2045, -2672, 15, 0 } }, { -39, 0, -4096, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 1408, 0, -225, 0 }, { { -2047, 2672, 11, 0 }, { 2030, 2672, -26, 0 }, { -2047, -2672, 11, 0 }, { 2030, -2672, -26, 0 } }, { 37, 0, 4112, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { -4320, 0, 1280, 0 }, { { -1410, 2672, 296, 0 }, { 1409, 2672, -297, 0 }, { -1410, -2672, 296, 0 }, { 1409, -2672, -297, 0 } }, { 843, 0, 4010, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -4002, 0, 1566, 0 }, { { 1745, 2672, -397, 0 }, { -1745, 2672, 397, 0 }, { 1745, -2672, -397, 0 }, { -1745, -2672, 397, 0 } }, { -910, 0, -3999, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -3040, 0, -2128, 0 }, { { -837, 2672, 8, 0 }, { 838, 2672, -8, 0 }, { -837, -2672, 8, 0 }, { 838, -2672, -8, 0 } }, { 37, 0, 4119, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -3040, 0, -1792, 0 }, { { 837, 2672, -8, 0 }, { -838, 2672, 8, 0 }, { 837, -2672, -8, 0 }, { -838, -2672, 8, 0 } }, { -40, 0, -4122, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1377, 0, -1697, 0 }, { { 819, 2672, 148, 0 }, { -829, 2672, -162, 0 }, { 819, -2672, 148, 0 }, { -829, -2672, -162, 0 } }, { 756, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1313, 0, -1984, 0 }, { { -829, 2672, -168, 0 }, { 814, 2672, 142, 0 }, { -829, -2672, -168, 0 }, { 814, -2672, 142, 0 } }, { -764, 0, 4040, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -529, 0, -3825, 0 }, { { -408, 2672, -342, 0 }, { 405, 2672, 335, 0 }, { -408, -2672, -342, 0 }, { 405, -2672, 335, 0 } }, { -2627, 0, 3148, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 4, 5, 129, 0 },
};

GpObj4C D_acropolis_cafeteria_80188C8C[18] = {
    { NULL, NULL, NULL, { -1458, -1872, 1406, 0 }, { { -1092, 2672, -566, 0 }, { 1084, 2672, 560, 0 }, { -1092, -2672, -566, 0 }, { 1084, -2672, 560, 0 } }, { -1889, 0, 3647, 0 }, { 0, 0, 4096, 0 }, 2930, 0, 2, 6, 1, 0 },
    { NULL, NULL, NULL, { -1538, 0, 1726, 0 }, { { 1286, 2672, 820, 0 }, { -1293, 2672, -825, 0 }, { 1286, -2672, 820, 0 }, { -1293, -2672, -825, 0 } }, { 2202, 0, -3454, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 2, 1, 0 },
    { NULL, NULL, NULL, { -4945, 0, -2273, 0 }, { { -1054, 2672, -199, 0 }, { 1047, 2672, 189, 0 }, { -1054, -2672, -199, 0 }, { 1047, -2672, 189, 0 } }, { -747, 0, 4036, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -4866, 0, -1921, 0 }, { { 1073, 2672, 165, 0 }, { -1077, 2672, -172, 0 }, { 1073, -2672, 165, 0 }, { -1077, -2672, -172, 0 } }, { 633, 0, -4048, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1442, 0, -4738, 0 }, { { 520, 2672, 1242, 0 }, { -521, 2672, -1243, 0 }, { 520, -2672, 1242, 0 }, { -521, -2672, -1243, 0 } }, { 3781, 0, -1586, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1313, 0, -5121, 0 }, { { -441, 2672, -1044, 0 }, { 434, 2672, 1037, 0 }, { -441, -2672, -1044, 0 }, { 434, -2672, 1037, 0 } }, { -3783, 0, 1588, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -817, 0, -3731, 0 }, { { 499, 2672, 345, 0 }, { -505, 2672, -353, 0 }, { 499, -2672, 345, 0 }, { -505, -2672, -353, 0 } }, { 2338, 0, -3366, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 1248, 0, 63, 0 }, { { 2033, 2672, -23, 0 }, { -2045, 2672, 15, 0 }, { 2033, -2672, -23, 0 }, { -2045, -2672, 15, 0 } }, { -39, 0, -4096, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 1408, 0, -225, 0 }, { { -2047, 2672, 11, 0 }, { 2030, 2672, -26, 0 }, { -2047, -2672, 11, 0 }, { 2030, -2672, -26, 0 } }, { 37, 0, 4112, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { -4320, 0, 1280, 0 }, { { -1410, 2672, 296, 0 }, { 1409, 2672, -297, 0 }, { -1410, -2672, 296, 0 }, { 1409, -2672, -297, 0 } }, { 843, 0, 4010, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -4002, 0, 1566, 0 }, { { 1745, 2672, -397, 0 }, { -1745, 2672, 397, 0 }, { 1745, -2672, -397, 0 }, { -1745, -2672, 397, 0 } }, { -910, 0, -3999, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -3040, 0, -2128, 0 }, { { -837, 2672, 8, 0 }, { 838, 2672, -8, 0 }, { -837, -2672, 8, 0 }, { 838, -2672, -8, 0 } }, { 37, 0, 4119, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -3040, 0, -1792, 0 }, { { 837, 2672, -8, 0 }, { -838, 2672, 8, 0 }, { 837, -2672, -8, 0 }, { -838, -2672, 8, 0 } }, { -40, 0, -4122, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1377, 0, -1697, 0 }, { { 819, 2672, 148, 0 }, { -829, 2672, -162, 0 }, { 819, -2672, 148, 0 }, { -829, -2672, -162, 0 } }, { 756, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1313, 0, -1984, 0 }, { { -829, 2672, -168, 0 }, { 814, 2672, 142, 0 }, { -829, -2672, -168, 0 }, { 814, -2672, 142, 0 } }, { -764, 0, 4040, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -529, 0, -3825, 0 }, { { -408, 2672, -342, 0 }, { 405, 2672, 335, 0 }, { -408, -2672, -342, 0 }, { 405, -2672, 335, 0 } }, { -2627, 0, 3148, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -1316, 0, -611, 0 }, { { -827, 2672, 1287, 0 }, { 818, 2672, -1292, 0 }, { -827, -2672, 1287, 0 }, { 818, -2672, -1292, 0 } }, { 3459, 0, 2206, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 3, 1, 0 },
    { NULL, NULL, NULL, { -1249, -64, -576, 0 }, { { 817, 2672, -1294, 0 }, { -829, 2672, 1285, 0 }, { 817, -2672, -1294, 0 }, { -829, -2672, 1285, 0 } }, { -3454, 0, -2205, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 6, 129, 0 },
};

GpObj4C D_acropolis_cafeteria_801891E4[16] = {
    { NULL, NULL, NULL, { -3984, -368, 2704, 0 }, { { -496, 0, -272, 0 }, { 496, 0, -272, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 565, 0, 3, 35, 2, 0 },
    { NULL, NULL, NULL, { -784, -360, -5440, 0 }, { { 320, 0, -544, 0 }, { 320, 0, 544, 0 }, { -320, 0, -544, 0 }, { -320, 0, 544, 0 } }, { 0, 4111, 0, 0 }, { -4096, 0, 0, 0 }, 630, 0, 7, 52, 2, 0 },
    { NULL, NULL, NULL, { 1952, -360, -3184, 0 }, { { 768, 0, 272, 0 }, { -768, 0, 272, 0 }, { 768, 0, -272, 0 }, { -768, 0, -272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, 0, 7, 19, 2, 0 },
    { NULL, NULL, NULL, { -2296, -401, -2424, 0 }, { { -1592, 0, -1272, 0 }, { 1096, 0, -888, 0 }, { -600, 0, 1576, 0 }, { 1096, 0, 584, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2035, 2, 7, 0, 4, 0 },
    { NULL, NULL, NULL, { 2720, -392, -2464, 0 }, { { -624, 0, -704, 0 }, { 624, 0, -704, 0 }, { -624, 0, 704, 0 }, { 624, 0, 704, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 940, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 1328, -360, 2160, 0 }, { { 464, 0, -608, 0 }, { 464, 0, 608, 0 }, { -464, 0, -608, 0 }, { -464, 0, 608, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 762, 2, 10, 4, 2, 0 },
    { NULL, NULL, NULL, { 40, -384, -1272, 0 }, { { -504, 0, 8, 0 }, { 552, 0, -1272, 0 }, { -408, 0, 1224, 0 }, { 1640, 0, 584, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 1736, 5, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { -3112, -384, 64, 0 }, { { -2744, 0, -464, 0 }, { -2744, 0, -944, 0 }, { 2728, 0, 944, 0 }, { 2760, 0, 464, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2896, 0x8005, 3, 0, 3, 0 },
    { NULL, NULL, NULL, { -5200, -384, 2784, 0 }, { { -416, 0, -976, 0 }, { 992, 0, -976, 0 }, { -416, 0, 368, 0 }, { 992, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1390, 2, 21, 0, 4, 0 },
    { NULL, NULL, NULL, { -992, -384, -2208, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 872, 2, 20, 0, 3, 0 },
    { NULL, NULL, NULL, { 480, -416, -2432, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 872, 2, 22, 0, 2, 0 },
    { NULL, NULL, NULL, { 2464, -384, -928, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 872, 2, 23, 0, 2, 0 },
    { NULL, NULL, NULL, { 2496, -384, 544, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4052, 0, 601, 0 }, 872, 2, 24, 0, 2, 0 },
    { NULL, NULL, NULL, { 2496, -384, 1761, 0 }, { { -496, 0, -304, 0 }, { 496, 0, -304, 0 }, { -496, 0, 304, 0 }, { 496, 0, 304, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4092, 0 }, 579, 2, 25, 0, 2, 0 },
    { NULL, NULL, NULL, { -2816, -364, 1872, 0 }, { { -240, 0, -1152, 0 }, { 752, 0, -1152, 0 }, { -752, 0, 1152, 0 }, { 240, 0, 1152, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1372, 0x8005, 3, 0, 3, 0 },
    { NULL, NULL, NULL, { -3520, -377, -1688, 0 }, { { -1047, 0, -123, 0 }, { 95, 0, -1086, 0 }, { -178, 0, 1212, 0 }, { 1130, 0, -3, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1221, 5, 0, 0, 132, 0 },
};

GpObj4C D_acropolis_cafeteria_801896A4[20] = {
    { NULL, NULL, NULL, { -3984, -368, 2704, 0 }, { { -496, 0, -272, 0 }, { 496, 0, -272, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 565, 0, 3, 35, 2, 0 },
    { NULL, NULL, NULL, { -784, -360, -5440, 0 }, { { 320, 0, -544, 0 }, { 320, 0, 544, 0 }, { -320, 0, -544, 0 }, { -320, 0, 544, 0 } }, { 0, 4111, 0, 0 }, { -4096, 0, 0, 0 }, 630, 0, 7, 52, 2, 0 },
    { NULL, NULL, NULL, { 1952, -360, -3184, 0 }, { { 768, 0, 272, 0 }, { -768, 0, 272, 0 }, { 768, 0, -272, 0 }, { -768, 0, -272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, 0, 7, 19, 2, 0 },
    { NULL, NULL, NULL, { -2296, -401, -2424, 0 }, { { -1592, 0, -1272, 0 }, { 1096, 0, -888, 0 }, { -600, 0, 1576, 0 }, { 1096, 0, 584, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2035, 2, 7, 0, 4, 0 },
    { NULL, NULL, NULL, { 2720, -392, -2464, 0 }, { { -624, 0, -704, 0 }, { 624, 0, -704, 0 }, { -624, 0, 704, 0 }, { 624, 0, 704, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 940, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 1328, -360, 2160, 0 }, { { 464, 0, -608, 0 }, { 464, 0, 608, 0 }, { -464, 0, -608, 0 }, { -464, 0, 608, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 762, 2, 10, 4, 2, 0 },
    { NULL, NULL, NULL, { 40, -384, -1272, 0 }, { { -504, 0, 8, 0 }, { 552, 0, -1272, 0 }, { -408, 0, 1224, 0 }, { 1640, 0, 584, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 1736, 5, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { -3112, -384, 64, 0 }, { { -2744, 0, -464, 0 }, { -2744, 0, -944, 0 }, { 2728, 0, 944, 0 }, { 2760, 0, 464, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2896, 0x8005, 3, 0, 3, 0 },
    { NULL, NULL, NULL, { -5200, -384, 2784, 0 }, { { -416, 0, -976, 0 }, { 992, 0, -976, 0 }, { -416, 0, 368, 0 }, { 992, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1390, 2, 21, 0, 4, 0 },
    { NULL, NULL, NULL, { -992, -384, -2208, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 872, 2, 20, 0, 3, 0 },
    { NULL, NULL, NULL, { 480, -416, -2432, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 872, 2, 22, 0, 2, 0 },
    { NULL, NULL, NULL, { 2464, -384, -928, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 872, 2, 23, 0, 2, 0 },
    { NULL, NULL, NULL, { 2496, -384, 544, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4052, 0, 601, 0 }, 872, 2, 24, 0, 2, 0 },
    { NULL, NULL, NULL, { 2496, -384, 1761, 0 }, { { -496, 0, -304, 0 }, { 496, 0, -304, 0 }, { -496, 0, 304, 0 }, { 496, 0, 304, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4092, 0 }, 579, 2, 25, 0, 2, 0 },
    { NULL, NULL, NULL, { -2816, -364, 1872, 0 }, { { -240, 0, -1152, 0 }, { 752, 0, -1152, 0 }, { -752, 0, 1152, 0 }, { 240, 0, 1152, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1372, 0x8005, 3, 0, 3, 0 },
    { NULL, NULL, NULL, { -3296, -377, -2296, 0 }, { { -1559, 0, 133, 0 }, { 95, 0, -414, 0 }, { -178, 0, 1756, 0 }, { 1130, 0, 317, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1764, 5, 0, 0, 4, 0 },
    { NULL, NULL, NULL, { -3480, -384, -1368, 0 }, { { -1272, 0, -792, 0 }, { 1320, 0, -632, 0 }, { -760, 0, 712, 0 }, { 712, 0, 712, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1498, 5, 0, 0, 2, 0 },
    { NULL, NULL, NULL, { -3496, -384, -2824, 0 }, { { 1272, 0, 792, 0 }, { -1320, 0, 632, 0 }, { 760, 0, -712, 0 }, { -712, 0, -712, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1498, 5, 0, 0, 2, 0 },
    { NULL, NULL, NULL, { -4034, -384, -2092, 0 }, { { -162, 0, -1428, 0 }, { 1220, 0, 15, 0 }, { -850, 0, -58, 0 }, { -205, 0, 1472, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1481, 5, 0, 0, 2, 0 },
    { NULL, NULL, NULL, { -3488, -384, -1728, 0 }, { { -240, 0, -720, 0 }, { 240, 0, -720, 0 }, { -240, 0, 720, 0 }, { 240, 0, 720, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 757, 5, 0, 0, 131, 0 },
};

GpObj3A D_acropolis_cafeteria_80189C94[2] = {
    { NULL, NULL, { -384, -304, -1136, 0 }, { { 0, 752, -4816, 0 }, { 0, 752, 4816, 0 }, { 0, -752, -4816, 0 }, { 0, -752, 4816, 0 } }, { -4106, 0, 0, 0 }, { -7, 18 }, 1, 0 },
    { NULL, NULL, { -352, -1456, -3920, 0 }, { { 0, 2208, -2400, 0 }, { 0, 2208, 2400, 0 }, { 0, -2208, -2400, 0 }, { 0, -2208, 2400, 0 } }, { -4099, 0, 0, 0 }, { -70, 12 }, 129, 0 },
};

GpAreaTmdRec D_acropolis_cafeteria_80189D0C[4] = {
    { 10, 106, 0, 0, { 0, 0 }, D_80148670 },
    { 29, 29, 1, 0, { 0, 0 }, D_80156E24 },
    { 102, 106, 2, 0, { 0, 0 }, D_801796A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_cafeteria_80189D3C[4] = {
    { 10, 106, 0, 0, { 0, 0 }, D_80148670 },
    { 19, 106, 1, 0, { 0, 0 }, D_8015A4EC },
    { 102, 106, 2, 0, { 0, 0 }, D_801796A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_cafeteria_80189D6C[2] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_cafeteria_80189D84[2] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_cafeteria_80189D9C[2] = {
    { 12, 12, 0, 0, { 0, 0 }, D_80138E98 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_cafeteria_80189DB4[2] = {
    { 57, 57, 0, 0, { 0, 0 }, D_801491F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_acropolis_cafeteria_80189DCC[11] = {
    { NULL, NULL },
    { D_map_akropolis_8017AE9C, D_acropolis_cafeteria_80189D0C },
    { D_map_akropolis_8017AEDC, D_acropolis_cafeteria_80189D3C },
    { D_map_akropolis_8017AF1C, D_acropolis_cafeteria_80189D6C },
    { D_map_akropolis_8017AF3C, D_acropolis_cafeteria_80189D84 },
    { D_map_akropolis_8017AF6C, D_acropolis_cafeteria_80189D9C },
    { NULL, NULL },
    { D_map_akropolis_8017AFCC, D_acropolis_cafeteria_80189DB4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

GpPointLight D_acropolis_cafeteria_80189E24[15] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1495, -3075, -3183 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3400, 3051, 2214, { 0, 0 } }, 1021, 1963 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2998, -2896, -1504 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2578, 2319, 1751, { 0, 0 } }, 3000, 0x2F46 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1010, -2896, -1501 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3380, 3031, 2354, { 0, 0 } }, 1221, 3424 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3140, -1728, -635 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3909, 1478, 540, { 0, 0 } }, 919, 2199 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2429, -1749, 2381 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4004, 4004, 4004, { 0, 0 } }, 299, 740 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1501, -2896, 222 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3540, 2951, 2234, { 0, 0 } }, 1417, 3524 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3223, -1918, 734 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4024, 4024, 4024, { 0, 0 } }, 1280, 2078 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2023, -2503, -3012 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3604, 4008, 4008, { 0, 0 } }, 962, 2040 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3012, -2896, 1693 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2759, 2230, 1690, { 0, 0 } }, 3584, 6227 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1656, -1336, 2316 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3746, 3353, 3359, { 0, 0 } }, 559, 1520 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3597, 84, -1634 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2285, 1654, 1243, { 0, 0 } }, 677, 3269 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4490, -2896, 220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2641, 2091, 1814, { 0, 0 } }, 360, 3524 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4997, -2896, -1499 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3365, 2996, 2286, { 0, 0 } }, 1219, 4243 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4483, -2896, -3170 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3420, 3031, 2232, { 0, 0 } }, 696, 2693 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2998, -2336, -4692 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3400, 2990, 2371, { 0, 0 } }, 698, 5980 },
};

AcropolisCafeteriaSpotLightStorage D_acropolis_cafeteria_8018A3C4 = {
    {
        { { { .coord = { 0, { { { -1629, -3, 3766 }, { 3759, -23, 1629 }, { 17, 4097, 9 } }, { -388, -2188, -10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4000, 3590, 721, { 0, 0 } }, { 3759, 1626, 9, 0 }, 1040, 3841, 910 },
    },
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x98,
        0xAE,
        0x1D,
        0x80,
        0x01,
        0x00,
        0x00,
        0x00,
        0x38,
        0xB4,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFF,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x48,
        0x00,
        0x38,
        0x00,
        0xC5,
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x48,
        0x00,
        0x00,
        0x00,
        0x48,
        0x00,
        0x40,
        0x00,
        0xA5,
        0x02,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x40,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x73,
        0x02,
        0x18,
        0x40,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x20,
        0x00,
        0x08,
        0x00,
        0x68,
        0x00,
        0x00,
        0x00,
        0x93,
        0x02,
        0x28,
        0xB0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x38,
        0x00,
        0x38,
        0x00,
        0x63,
        0x03,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x18,
        0x00,
        0x48,
        0x00,
        0x38,
        0x00,
        0x63,
        0x03,
        0x70,
        0xD8,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x08,
        0x00,
        0x78,
        0x00,
        0x30,
        0x00,
        0x02,
        0x03,
        0x30,
        0x60,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x10,
        0x00,
        0x70,
        0x00,
        0x38,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x18,
        0x00,
        0x18,
        0x00,
        0x68,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x18,
        0x00,
        0x18,
        0x00,
        0x80,
        0x00,
        0x00,
        0x00,
        0x60,
        0x03,
        0x48,
        0xA0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x38,
        0x00,
        0x48,
        0x00,
        0xBC,
        0x02,
        0x48,
        0x70,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x00,
        0x00,
        0x38,
        0x00,
        0x50,
        0x00,
        0xB9,
        0x02,
        0x40,
        0xC0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x02,
        0x38,
        0x68,
        0x80,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x08,
        0x00,
        0x48,
        0x00,
        0x00,
        0x00,
        0x89,
        0x02,
        0x38,
        0xA8,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x08,
        0x00,
        0x30,
        0x00,
        0x58,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x08,
        0x00,
        0x30,
        0x00,
        0x60,
        0x00,
        0x98,
        0x02,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x10,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x38,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x68,
        0x02,
        0x18,
        0x00,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x00,
        0x00,
        0xF0,
        0xFF,
        0xF0,
        0xFF,
        0xD5,
        0x03,
        0x28,
        0x10,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x30,
        0x00,
        0xF0,
        0xFF,
        0x00,
        0x00,
        0x19,
        0x04,
        0x50,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x18,
        0x00,
        0x28,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xEF,
        0x03,
        0x48,
        0x78,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x50,
        0x00,
        0xE0,
        0xFF,
        0xFB,
        0x04,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x00,
        0x00,
        0x58,
        0x00,
        0xE0,
        0xFF,
        0xB0,
        0x04,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x40,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x65,
        0x04,
        0x70,
        0x98,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x40,
        0x00,
        0x78,
        0x00,
        0x00,
        0x00,
        0x1A,
        0x04,
        0x68,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x88,
        0x00,
        0xE8,
        0xFF,
        0x9D,
        0x03,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x50,
        0x00,
        0x98,
        0x00,
        0xF8,
        0xFF,
        0x84,
        0x03,
        0x78,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x08,
        0x00,
        0x78,
        0x00,
        0xE8,
        0xFF,
        0xE8,
        0x03,
        0x40,
        0xB8,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x20,
        0x00,
        0x08,
        0x00,
        0x68,
        0x00,
        0xE0,
        0xFF,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x08,
        0x00,
        0x60,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x20,
        0x00,
        0x70,
        0x00,
        0x00,
        0x00,
        0xE8,
        0x03,
        0x78,
        0xD8,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x78,
        0x00,
        0x88,
        0xFF,
        0xCF,
        0x03,
        0x60,
        0xB0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x00,
        0x00,
        0x88,
        0x00,
        0x88,
        0xFF,
        0xB6,
        0x03,
        0x58,
        0x40,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0E,
        0x00,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x05,
        0x00,
        0x18,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x03,
        0x00,
        0x1B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x07,
        0x00,
        0x28,
        0x00,
        0x03,
        0x00,
        0x00,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x20,
        0x00,
        0xE0,
        0xFF,
        0xD8,
        0xFF,
        0xE6,
        0x06,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x20,
        0x00,
        0x10,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x20,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB4,
        0x05,
        0x20,
        0x00,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x00,
        0x00,
        0xF8,
        0xFF,
        0xF0,
        0xFF,
        0x54,
        0x06,
        0x30,
        0x30,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x18,
        0x00,
        0x08,
        0x00,
        0xF0,
        0xFF,
        0x41,
        0x06,
        0x78,
        0xE8,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x20,
        0x00,
        0xF8,
        0xFF,
        0xF0,
        0xFF,
        0x55,
        0x05,
        0x50,
        0xB0,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0xF8,
        0xFF,
        0x27,
        0x05,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x18,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x08,
        0x00,
        0x5A,
        0x05,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x20,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x6B,
        0x03,
        0x30,
        0xD0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x08,
        0x00,
        0x08,
        0x00,
        0xD0,
        0xFF,
        0x00,
        0x00,
        0x0E,
        0x04,
        0x68,
        0xB0,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8E,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD0,
        0xFF,
        0x20,
        0x00,
        0x0B,
        0x04,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x30,
        0x00,
        0x10,
        0x00,
        0xA8,
        0xFF,
        0x28,
        0x00,
        0x78,
        0x03,
        0x18,
        0x90,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x10,
        0x00,
        0x10,
        0x00,
        0xD8,
        0xFF,
        0x28,
        0x00,
        0x1C,
        0x04,
        0x38,
        0x70,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x10,
        0x00,
        0xB0,
        0xFF,
        0x38,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x18,
        0x00,
        0x08,
        0x00,
        0xE0,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0x00,
        0x00,
        0x20,
        0x00,
        0x10,
        0x00,
        0xD8,
        0xFF,
        0x00,
        0x00,
        0x9A,
        0x03,
        0x18,
        0x70,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x00,
        0x00,
        0x00,
        0x00,
        0xE0,
        0xFF,
        0x30,
        0x00,
        0xE3,
        0x03,
        0x30,
        0x80,
        0x80,
        0x80,
        0x80,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x20,
        0x00,
        0x00,
        0x00,
        0x40,
        0x00,
        0x20,
        0x00,
        0x7E,
        0x03,
        0x30,
        0xB8,
        0x80,
        0x80,
        0x80,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x24,
        0x03,
        0x08,
        0x18,
        0x80,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x28,
        0x00,
        0x18,
        0x00,
        0x78,
        0x00,
        0x00,
        0x00,
        0xB0,
        0x02,
        0x20,
        0x40,
        0x80,
        0x80,
        0x00,
        0x00,
        0x8F,
        0x00,
        0xC0,
        0x3F,
        0x28,
        0x00,
        0x10,
        0x00,
        0x70,
        0x00,
        0x50,
        0x00,
    },
};

static inline s32 _acropolisCafeteriaAnswer(RoomEventMsg* in, RoomEventMsg* out);

static void func_acropolis_cafeteria_8017D6AC(Task* task)
{
    if (gDisplayState.field_112 != 0) {
        func_80724608(gameGetPtrSlot(3), -0x8C, -0x32, (void*)CafeteriaPlayerLabel);
        func_807245E4(gameGetPtrSlot(3));
    }
}

/// Answers message 3 in the outgoing copy unless field_5 suppresses it: before
/// nibble 0 reaches 2 the answer is 1 or 2 by nibble 0x21, afterwards the
/// message id itself. Returns 1, the handler's "message accepted" result.
static inline s32 _acropolisCafeteriaAnswer(RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId = in->prefix.packed;

    if (msgId == 3 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0) < 2) {
            if (GameFlag_GetNibble(0x21) < 2) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        } else {
            out->field_3 = msgId;
        }
    }
    return 1;
}

/// Copies the room message, selects its response, and starts capture slots 5
/// or 6 when the room's progress permits. field_5 suppresses side effects.
s32 func_acropolis_cafeteria_8017D700(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;

    *out = *in;
    if (in->prefix.packed == 7 && in->field_2 == 4) {
        if (GameFlag_GetNibble(0) >= 3) {
            return 1;
        }
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_StartCapSlot(5, 1, 0);
        }
        return 0;
    }
    msgId = in->prefix.packed;
    if (msgId == 3) {
        if (GameFlag_GetNibble(0) < 2) {
            if (D_acropolis_cafeteria_80184164 == 0) {
                return _acropolisCafeteriaAnswer(in, out);
            }
            if (D_acropolis_cafeteria_80184164 == 2) {
                if (in->field_5 == 0) {
                    Gp_StartCapSlot(6, 1, 0);
                }
            }
            return 0;
        }
        if (GameFlag_GetNibble(0xE) == msgId && in->field_5 == 0) {
            GameFlag_SetNibble(0xE, 2);
        }
        return _acropolisCafeteriaAnswer(in, out);
    }
    return 1;
}

/// Scripted-event task for this room, one step per `task->state`. Most states
/// advance by one; state 8 jumps to 14, states 9-13 are never reached that way
/// and idle. States 2-8 and 14 raise `blackout`, which covers the whole frame
/// with a black `TILE` linked into ordering-table slot 10. State 27 ends the
/// sequence by killing the task once the slot-3 object accepts message 0x3ED.
void func_acropolis_cafeteria_8017D8F8(Task* task)
{
    u8    param1[4];
    u8    param2[4];
    TILE* tile;
    u8    blackout;

    blackout = 0;
    switch (task->state) {
        case 0:
            gGameSession->flowFlags = 3;
            func_800E8634(D_acropolis_cafeteria_80182E74, 1, D_acropolis_cafeteria_801831BC);
            task->state += 1;
            break;
        case 1:
            if (gGameSession->eventState != 1) {
                task->state += 1;
            }
            break;
        case 2:
            blackout                   = 1;
            gGameSession->at4.loc.room = Mc_SaveData[0].state.at4.loc.room = 2;
            gGameSession->roomObjsDirty                                    = 1;
            task->state                                                   += 1;
            break;
        case 3:
            blackout = 1;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7D9, 0, 0);
            task->state += 1;
            break;
        case 4:
            blackout     = 1;
            task->state += 1;
            break;
        case 5:
            blackout = 1;
            func_800A99B4();
            task->state += 1;
            break;
        case 6:
            blackout  = 1;
            param1[2] = 0x15;
            param1[3] = 0;
            param1[0] = 0;
            param2[0] = 6;
            param2[1] = 0;
            param2[2] = 4;
            param2[3] = 6;
            CdCmd_Enqueue(0x21, param1, param2);
            task->state += 1;
            break;
        case 7:
            blackout = 1;
            if (CdCmd_IsIdle()) {
                Gp_SetAreaObjId(&Mc_SaveData[0].state.at4.loc, 2, 1);
                Gp_SyncAreaKeyIndex(&Mc_SaveData[0].state.at4.loc);
                Gp_SpawnArea(&Mc_SaveData[0].state.at4.loc);
                D_801156A4  &= ~0x40;
                task->state += 1;
            }
            break;
        case 8:
            blackout = 1;
            if (gDisplayState.field_112 != -1) {
                Task_SpawnFromTable(D_acropolis_cafeteria_80184178, 0, 0, 0);
            }
            task->state = 14;
            break;
        case 14:
            blackout = 1;
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case 15:
            func_800E8614(D_acropolis_cafeteria_80183F3C, 0);
            task->state += 1;
            break;
        case 16:
        case 20:
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case 18:
            if (Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0 && Player_Status.hp > 0 && Gp_StateC08.field_A != 1 &&
                gDisplayState.pendingMode == 0) {
                Gp_MsgPlayerWeapon(0);
                task->state += 1;
            }
            break;
        case 19:
            Display_ClampField126(0);
            func_800E8634(D_acropolis_cafeteria_8018330C, 0, D_acropolis_cafeteria_801834D4);
            task->state += 1;
            break;
        case 21:
            Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0xA);
            gGameSession->flowFlags        |= 0x80;
            Gp_StateF0.prefix.bytes.field_1 = 3;
            D_acropolis_cafeteria_80184164  = 2;
            task->state                    += 1;
            break;
        case 17:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
            task->state += 1;
            break;
        case 27:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                func_800E3FAC(0xA2, 3);
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
    }
    if (blackout != 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = (void*)(tile + 1);
        SetTile(tile);
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        addPrim(&gGpuCurrentOt[10], tile);
    }
}

void func_acropolis_cafeteria_8017DD1C(Task* task)
{
    char pad[8];

    switch (task->state) {
        case 0:
            if (Gp_GetCurBit2Flag(3) == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
                task->state = task->state + 1;
            } else {
                taskKill(task);
            }
            break;

        case 1:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_RunCapCmd1(3);
                task->state = task->state + 1;
            }
            break;

        case 2:
            if (Gp_CapBusy() == 0) {
                if (Gp_GetCurBit2Flag(3) == 1) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
                    task->state = task->state + 1;
                } else {
                    task->state = 6;
                }
            }
            break;

        case 3:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;

        case 6:
            func_800E8634(D_acropolis_cafeteria_8018363C, 0, D_acropolis_cafeteria_80183DBC);
            task->state = task->state + 1;
            break;

        case 7:
            if (gGameSession->eventState != 1) {
                task->state = task->state + 1;
            }
            break;

        case 8:
            GameFlag_SetNibble(0, 2);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 4);
            GameFlag_SetNibble(0xE, 1);
            Gp_ApplyAreaRecs(D_acropolis_cafeteria_8018C9D4);
            Mc_SaveData[0].state.sceneEvent = 4;
            func_800E3FAC(0xA2, 4);
            func_800ABFF8();
            func_800AC000();
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData[0].state.at4.loc.stage = 1;
            Mc_SaveData[0].state.at4.loc.area  = 3;
            Mc_SaveData[0].state.at4.loc.warp  = 3;
            Mc_SaveData[0].state.at4.loc.room  = 3;
            gDisplayState.roomVariant          = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

void func_acropolis_cafeteria_8017DF68(Task* task)
{
    GpCoord* coord;

    coord = Gp_LookupSlot4(0)->extra.tmd->coords;
    switch (task->state) {
        case 0:
            Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7D4, &D_acropolis_cafeteria_80182D28, 0);
            Gp_DispatchMsgPtr(Gp_LookupSlot4(0), 0x7DB, &D_acropolis_cafeteria_80182DB8, 0);
            D_acropolis_cafeteria_8018D6A0 = 0;
            D_acropolis_cafeteria_8018D6A4 = -0x14;
            D_acropolis_cafeteria_8018D6A8 = -0x14;
            task->state                    = task->state + 1;
            break;

        case 1:
            coord->coord.t[0] += D_acropolis_cafeteria_8018D6A0;
            coord->coord.t[1] += D_acropolis_cafeteria_8018D6A4;
            if (coord->coord.t[1] > -0x12C) {
                coord->coord.t[1] = -0x12C;
            }
            coord->coord.t[2]              += D_acropolis_cafeteria_8018D6A8;
            coord->flg                      = 0;
            D_acropolis_cafeteria_8018D6A0  = D_acropolis_cafeteria_8018D6A0 / 2;
            D_acropolis_cafeteria_8018D6A4 += 5;
            if (D_acropolis_cafeteria_8018D6A4 > 0x14) {
                D_acropolis_cafeteria_8018D6A4 = 0x14;
            }
            D_acropolis_cafeteria_8018D6A8 = D_acropolis_cafeteria_8018D6A8 / 2;
            if (D_acropolis_cafeteria_8018D6A0 == 0 && D_acropolis_cafeteria_8018D6A8 == 0 &&
                coord->coord.t[1] == -0x12C) {
                taskKill(task);
            }
            break;
    }
}

/// Message handler that accepts its message and does nothing else.
s32 func_acropolis_cafeteria_8017E0D4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_acropolis_cafeteria_8017E0DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (GameFlag_GetNibble(0) >= 2 || D_acropolis_cafeteria_80184164 >= 2) {
            if (Gp_GetCurBit2Flag(4) == 1 || Gp_GetCurBit2Flag(4) == 0) {
                Gp_StartCapSlot(7, 1, 0);
            }
        }
    }
    return 0;
}
/// Handler for slot-7 msg `0x13EF`: the directed action selected by `field_2`.
s32 func_acropolis_cafeteria_8017E154(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0) {
        if (D_acropolis_cafeteria_80184164 >= 2 || GameFlag_GetNibble(0) >= 2) {
            Task_SpawnFromTable(D_acropolis_cafeteria_80182AD8, 1, 0, 0);
            return 0;
        }
    }
    if (arg2->field_2 == 2) {
        Gp_RunCapCmd1(9);
    } else if (arg2->field_2 == 3) {
        if (D_acropolis_cafeteria_80184164 == 0 && GameFlag_GetNibble(0) == 1) {
            D_acropolis_cafeteria_80184164 = 1;
            Task_SpawnFromTable(D_acropolis_cafeteria_80182AD8, 0, 0, 0);
        }
    }
    return 0;
}
s32 func_acropolis_cafeteria_8017E22C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0xA:
            SndEvt_EnqueueType6(0x5104000A, 0, 0);
            break;
        case 0xB:
            SndEvt_EnqueueType6(0x5104000B, 0, 0);
            break;
    }
    return 0;
}
void func_acropolis_cafeteria_8017E27C(s32 arg0)
{
    ((GameActor*)(gameGetPtrSlot(3))->work)->field_930 = arg0;
}

void func_acropolis_cafeteria_8017E2B0(void)
{
    Gp_PulseState1C80();
}

void func_acropolis_cafeteria_8017E2D0(void)
{
    MoveImage(&D_acropolis_cafeteria_80184168, 0x180, 0x100);
    MoveImage(&D_acropolis_cafeteria_80184170, 0, 0xF7);
}

void func_acropolis_cafeteria_8017E310(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

static void func_acropolis_cafeteria_8017E348(Task* task)
{
    task->msgTable = D_acropolis_cafeteria_80182AA8;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0) == 1) {
        Gp_MsgSlot4Chain(0, 0);
        Gp_MsgSlot4Chain(1, 1);
    } else if (GameFlag_GetNibble(0) == 2) {
        Gp_MsgSlot4Chain(0, 1);
        Gp_MsgSlot4Chain(1, 2);
        Gp_MsgSlot4Chain(2, 1);
        (D_acropolis_cafeteria_801891E4 + 9)[0].field_4A &= 0xBF;
        Gp_DispatchMsgPtr(Gp_LookupSlot4(2), 0x7D4, &D_acropolis_cafeteria_80182DDC, 0);
    }
    task->state = task->state + 1;
}

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_cafeteria_8017E424(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_cafeteria_8017D5C4;
    sp.funcs[task->state](task);
}
