#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Reset argument the first enemy's "play animation" opcode leaves behind:
/// `func_actor_535700_80132730` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 D_actor_535700_8013DAA8;

/// Fade countdown. `func_actor_535700_80131EF0` seeds it from its argument and
/// spawns the fade task from `D_actor_535700_8013346C`; that task
/// (`func_actor_535700_80131E24`) draws a full-screen black `TILE` into
/// ordering table slot 0xA while the count is non-zero, kills itself once it
/// reaches zero, and decrements the count every frame.
extern s32 D_actor_535700_80146840;

/// The first enemy's work block, published by its spawn handler.
extern Actor151000Work* D_actor_535700_80146844;

/// The first enemy's task, published by its spawn handler so the message
/// handlers can reach its model.
extern Task* D_actor_535700_80146848;

/// Picks the distance `func_actor_535700_80132108` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_535700_8014684C;

/// Descriptor of the fade task `func_actor_535700_80131E24`.
extern TaskDesc D_actor_535700_8013346C;

/// The first enemy's message table and the animation data its work block's
/// slots are seeded from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpAnimArg*);
        s32 (*call2)(Task*, s32, GpAnimArg*, s32);
        s32 (*call3)(Task*, s32, GpCmdArg*);
        s32 (*call4)(Task*, s32, GpXformArg*);
        s32 (*call5)(Task*, s32, VECTOR*);
        s32 (*call6)(Task*, s32, VECTOR*, s32);
        s32 (*call7)(Task*, s32, s32);
    } handler;
} Actor535700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor535700MsgEntry, 8);

extern Actor535700MsgEntry D_actor_535700_8013DAAC[];
extern u8                  D_actor_535700_8013DAE8[];

/// The second enemy's message table, the `TaskDesc` table its sub-model task
/// comes from, and the animation data its work block's slots are seeded from.
extern Actor535700MsgEntry D_actor_535700_801467E0[];
extern TaskDesc            D_actor_535700_80146810[];
extern u8                  D_actor_535700_80146828[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void func_actor_535700_80132108(Task* task);
static void func_actor_535700_801324D4(GpEnemy* enemy, Task* task);
static void func_actor_535700_80132558(Task* task);
static void func_actor_535700_80132580(Task* task);
static void func_actor_535700_80132648(void);
static void func_actor_535700_80132694(void);
static void func_actor_535700_80132730(void);
static void func_actor_535700_80132ABC(Task* task);
static void func_actor_535700_80132D68(Task* task);
static void func_actor_535700_80132F74(GpEnemy* enemy, Task* task);
static void func_actor_535700_80132FF8(Task* task);
static void func_actor_535700_80133020(Task* task);
static void func_actor_535700_801330BC(Task* task);
static void func_actor_535700_80133108(Task* task);
static void func_actor_535700_80133180(Task* task);

extern TmdSource D_actor_535700_80139A6C;
void             func_actor_535700_80132478(Task*);

s32 func_actor_535700_801327BC(Task*, s32, GpAnimArg*, s32);
s32 func_actor_535700_8013284C(Task*, s32, s32);
s32 func_actor_535700_80132894(Task*, s32, GpXformArg*);
s32 func_actor_535700_80132910(Task*, s32, GpCmdArg*);
s32 func_actor_535700_80132960(Task*, s32, VECTOR*, s32);

extern TmdSource D_actor_535700_80142E58;
extern TmdSource D_actor_535700_8014339C;
s32              func_actor_535700_801331E4(Task*, s32, GpAnimArg*);
s32              func_actor_535700_80133250(Task*, s32, s32);
s32              func_actor_535700_801332B4(Task*, s32, GpXformArg*);
s32              func_actor_535700_8013332C(void);
s32              func_actor_535700_80133334(Task*, s32, VECTOR*);
void             func_actor_535700_80132F20(Task*);
void             func_actor_535700_801333FC(Task*);

void func_actor_535700_80131EF0(s32);
void func_actor_535700_80131F2C(void);

extern GpAnimArg  D_actor_535700_80133478;
extern GpAnimArg  D_actor_535700_801334A4;
extern GpAnimArg  D_actor_535700_801334B8;
extern GpAnimArg  D_actor_535700_801334CC;
extern GpAnimArg  D_actor_535700_801334F4;
extern GpAnimArg  D_actor_535700_80133508;
extern GpAnimArg  D_actor_535700_8013351C;
extern GpAnimArg  D_actor_535700_80133530;
extern GpAnimArg  D_actor_535700_80133544;
extern GpAnimArg  D_actor_535700_80133558;
extern GpAnimArg  D_actor_535700_8013356C;
extern GpAnimArg  D_actor_535700_80133580;
extern GpAnimArg  D_actor_535700_80133594;
extern GpAnimArg  D_actor_535700_801335A8;
extern GpAnimArg  D_actor_535700_80133684;
extern GpCmdArg   D_actor_535700_8013348C;
extern GpXformArg D_actor_535700_80133698;
extern GpXformArg D_actor_535700_801336B0;
extern GpXformArg D_actor_535700_801336C8;
extern GpXformArg D_actor_535700_801336E0;
extern GpXformArg D_actor_535700_801336F8;
extern GpXformArg D_actor_535700_80133710;
extern GpXformArg D_actor_535700_80133728;
extern GpXformArg D_actor_535700_80133740;
extern GpXformArg D_actor_535700_80133758;
extern GpXformArg D_actor_535700_80133770;
extern GpXformArg D_actor_535700_80133788;
extern GpXformArg D_actor_535700_801337A0;
extern GpXformArg D_actor_535700_801337B8;
extern GpXformArg D_actor_535700_801337D0;
extern GpXformArg D_actor_535700_801337E8;
extern GpXformArg D_actor_535700_80133800;
extern GpXformArg D_actor_535700_80133818;
void              func_actor_535700_80131EF0(s32);
void              func_actor_535700_80131F2C(void);

void func_actor_535700_80131E24(Task*);

TaskDesc D_actor_535700_8013346C = { 0, 192, func_actor_535700_80131E24, { .model = NULL } };

GpAnimArg D_actor_535700_80133478 = { { .index = 1 }, 1, 0, 0, 0 };

GpCmdArg D_actor_535700_8013348C = { { .loc = { 3, 8 } }, 0 };

GpAnimArg D_actor_535700_80133490 = { { .index = 0 }, 0, 0, 0, 0 };

GpAnimArg D_actor_535700_801334A4 = { { .index = 0 }, 1, 1, 8, 0 };

GpAnimArg D_actor_535700_801334B8 = { { .index = 0 }, 2, 0, 0, 0 };

GpAnimArg D_actor_535700_801334CC = { { .index = 0 }, 3, 1, 8, 0 };

GpAnimArg D_actor_535700_801334E0 = { { .index = 0 }, 4, 0, 0, 0 };

GpAnimArg D_actor_535700_801334F4 = { { .index = 0 }, 5, 0, 0, 0 };

GpAnimArg D_actor_535700_80133508 = { { .index = 0 }, 6, 1, 8, 0 };

GpAnimArg D_actor_535700_8013351C = { { .index = 0 }, 7, 0, 0, 0 };

GpAnimArg D_actor_535700_80133530 = { { .index = 0 }, 8, 0, 0, 0 };

GpAnimArg D_actor_535700_80133544 = { { .index = 0 }, 10, 0, 0, 0 };

GpAnimArg D_actor_535700_80133558 = { { .index = 0 }, 13, 0, 0, 0 };

GpAnimArg D_actor_535700_8013356C = { { .index = 0 }, 14, 0, 0, 0 };

GpAnimArg D_actor_535700_80133580 = { { .index = 0 }, 15, 0, 0, 0 };

GpAnimArg D_actor_535700_80133594 = { { .index = 0 }, 4, 1, 8, 0 };

GpAnimArg D_actor_535700_801335A8 = { { .index = 0 }, 1, 0, 0, 0 };

GpAnimArg D_actor_535700_801335BC[10] = {
    { { .index = 0 }, 2, 0, 0, 0 },
    { { .index = 0 }, 3, 1, 8, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
    { { .index = 0 }, 1, 0, 0, 0 },
};

GpAnimArg D_actor_535700_80133684 = { { .index = 0 }, 4, 0, 0, 0 };

GpXformArg D_actor_535700_80133698 = { { 0x7530, 0, 0x7530, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_801336B0 = { { 50, 0, -0x37C8, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_801336C8 = { { 50, 0, -0x2CC4, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_801336E0 = { { 50, 0, -0x2CC4, 0 }, { 0, -398, 0, 0 } };

GpXformArg D_actor_535700_801336F8 = { { 0, 0, -0x2DC8, 0 }, { 0, -2048, 0, 0 } };

GpXformArg D_actor_535700_80133710 = { { 0, 0, -0x2710, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_80133728 = { { -1300, 0, -0x2710, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_80133740 = { { 0, 0, -0x2710, 0 }, { 0, -2048, 0, 0 } };

GpXformArg D_actor_535700_80133758 = { { -300, 0, -0x283C, 0 }, { 0, -1991, 0, 0 } };

GpXformArg D_actor_535700_80133770 = { { 0, 0, -0x4650, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_80133788 = { { 0, 0, -0x3BC4, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_actor_535700_801337A0 = { { -3670, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_actor_535700_801337B8 = { { -400, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_actor_535700_801337D0 = { { 3670, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_actor_535700_801337E8 = { { 400, 0, -0x2710, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_actor_535700_80133800 = { { 0, 0, -500, 0 }, { 0, -2048, 0, 0 } };

GpXformArg D_actor_535700_80133818 = { { 0, 0, -4500, 0 }, { 0, -2048, 0, 0 } };

GpXformArg D_actor_535700_80133830[2] = {
    { { 3500, 0, -2000, 0 }, { 0, -1024, 0, 0 } },
    { { 0, 0, -2000, 0 }, { 0, -1024, 0, 0 } },
};

GpXformArg D_actor_535700_80133860 = { { 0, 0, -0x38A4, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_actor_535700_80133878 = { { 0, 0, -0x3322, 0 }, { 0, -1024, 0, 0 } };

GpOverlayIds D_actor_535700_80133890 = { 3, 57, 11 };

GpEvsCmd D_actor_535700_80133898[99] = {
    { 12, { .overlays = &D_actor_535700_80133890 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_535700_80133698 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_535700_80133478 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_535700_801336B0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_8013356C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_535700_801336C8 }, { .value = 2 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133558 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_535700_801336C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133544 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133558 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_535700_801336E0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133544 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_801334A4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_535700_80133770 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_535700_801335A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_535700_80133594 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2013 }, { .storage = &D_actor_535700_80133788 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_535700_801336F8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_801334B8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_535700_80133710 }, { .value = 1 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_801334CC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2011 }, { .storage = &D_actor_535700_8013348C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133580 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_535700_80133728 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_801334B8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_535700_80133710 }, { .value = 1 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_535700_801337A0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_535700_80133684 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2013 }, { .storage = &D_actor_535700_801337B8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_actor_535700_801337D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_535700_80133684 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2013 }, { .storage = &D_actor_535700_801337E8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_actor_535700_801337D0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_535700_80133684 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2013 }, { .storage = &D_actor_535700_801337E8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_535700_80133800 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_535700_80133684 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2013 }, { .storage = &D_actor_535700_80133818 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2004 }, { .storage = &D_actor_535700_80133860 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2003 }, { .storage = &D_actor_535700_80133684 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2013 }, { .storage = &D_actor_535700_80133878 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_535700_80133740 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_8013351C }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133530 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_535700_80133758 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_801334F4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_535700_80133508 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 1 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_535700_80131EF0 }, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_535700_80131F2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_535700_80131EF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_535700_801341E0[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_535700_80131EF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_535700_80131F2C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_535700_801342B8[19] = {
#include "assets/actor_535700_model_07C4C_skeleton.inc"
};

u32 D_actor_535700_80134564[19] = {
#include "assets/actor_535700_model_07C4C_partVerts.inc"
};

SVECTOR D_actor_535700_801345B0[365] = {
#include "assets/actor_535700_model_07C4C_verts.inc"
};

SVECTOR D_actor_535700_80135118[385] = {
#include "assets/actor_535700_model_07C4C_normals.inc"
};

u32 D_actor_535700_80135D20[3923] = {
#include "assets/actor_535700_model_07C4C_stream.inc"
};

TmdSource D_actor_535700_80139A6C = {
    0,
    21760,
    5992,
    19,
    D_actor_535700_80134564,
    D_actor_535700_801345B0,
    D_actor_535700_80135118,
    D_actor_535700_801342B8,
    D_actor_535700_80135D20,
};

GpPackedPose D_actor_535700_80139A90[2] = {
#include "assets/actor_535700_animation_07E5C_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80139AA8[23] = {
#include "assets/actor_535700_animation_07E5C_bank4.inc"
};

GpAnimRec D_actor_535700_80139B04[84] = {
#include "assets/actor_535700_animation_07E5C_records.inc"
};

u16 D_actor_535700_80139C54[20] = {
#include "assets/actor_535700_animation_07E5C_indices.inc"
};

GpAnimSet D_actor_535700_80139C7C = {
    D_actor_535700_80139B04,
    D_actor_535700_80139C54,
    { NULL, D_actor_535700_80139A90, NULL, NULL, D_actor_535700_80139AA8, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_80139CA4[7] = {
#include "assets/actor_535700_animation_08254_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80139CF8[75] = {
#include "assets/actor_535700_animation_08254_bank4.inc"
};

GpAnimRec D_actor_535700_80139E24[138] = {
#include "assets/actor_535700_animation_08254_records.inc"
};

u16 D_actor_535700_8013A04C[20] = {
#include "assets/actor_535700_animation_08254_indices.inc"
};

GpAnimSet D_actor_535700_8013A074 = {
    D_actor_535700_80139E24,
    D_actor_535700_8013A04C,
    { NULL, D_actor_535700_80139CA4, NULL, NULL, D_actor_535700_80139CF8, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013A09C[22] = {
#include "assets/actor_535700_animation_08E38_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013A1A4[298] = {
#include "assets/actor_535700_animation_08E38_bank4.inc"
};

GpAnimRec D_actor_535700_8013A64C[377] = {
#include "assets/actor_535700_animation_08E38_records.inc"
};

u16 D_actor_535700_8013AC30[20] = {
#include "assets/actor_535700_animation_08E38_indices.inc"
};

GpAnimSet D_actor_535700_8013AC58 = {
    D_actor_535700_8013A64C,
    D_actor_535700_8013AC30,
    { NULL, D_actor_535700_8013A09C, NULL, NULL, D_actor_535700_8013A1A4, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013AC80[4] = {
#include "assets/actor_535700_animation_090A0_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013ACB0[48] = {
#include "assets/actor_535700_animation_090A0_bank4.inc"
};

GpAnimRec D_actor_535700_8013AD70[74] = {
#include "assets/actor_535700_animation_090A0_records.inc"
};

u16 D_actor_535700_8013AE98[20] = {
#include "assets/actor_535700_animation_090A0_indices.inc"
};

GpAnimSet D_actor_535700_8013AEC0 = {
    D_actor_535700_8013AD70,
    D_actor_535700_8013AE98,
    { NULL, D_actor_535700_8013AC80, NULL, NULL, D_actor_535700_8013ACB0, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013AEE8[5] = {
#include "assets/actor_535700_animation_094A8_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013AF24[76] = {
#include "assets/actor_535700_animation_094A8_bank4.inc"
};

GpAnimRec D_actor_535700_8013B054[147] = {
#include "assets/actor_535700_animation_094A8_records.inc"
};

u16 D_actor_535700_8013B2A0[20] = {
#include "assets/actor_535700_animation_094A8_indices.inc"
};

GpAnimSet D_actor_535700_8013B2C8 = {
    D_actor_535700_8013B054,
    D_actor_535700_8013B2A0,
    { NULL, D_actor_535700_8013AEE8, NULL, NULL, D_actor_535700_8013AF24, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013B2F0[5] = {
#include "assets/actor_535700_animation_098B4_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013B32C[89] = {
#include "assets/actor_535700_animation_098B4_bank4.inc"
};

GpAnimRec D_actor_535700_8013B490[135] = {
#include "assets/actor_535700_animation_098B4_records.inc"
};

u16 D_actor_535700_8013B6AC[20] = {
#include "assets/actor_535700_animation_098B4_indices.inc"
};

GpAnimSet D_actor_535700_8013B6D4 = {
    D_actor_535700_8013B490,
    D_actor_535700_8013B6AC,
    { NULL, D_actor_535700_8013B2F0, NULL, NULL, D_actor_535700_8013B32C, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013B6FC[4] = {
#include "assets/actor_535700_animation_09BC4_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013B72C[59] = {
#include "assets/actor_535700_animation_09BC4_bank4.inc"
};

GpAnimRec D_actor_535700_8013B818[105] = {
#include "assets/actor_535700_animation_09BC4_records.inc"
};

u16 D_actor_535700_8013B9BC[20] = {
#include "assets/actor_535700_animation_09BC4_indices.inc"
};

GpAnimSet D_actor_535700_8013B9E4 = {
    D_actor_535700_8013B818,
    D_actor_535700_8013B9BC,
    { NULL, D_actor_535700_8013B6FC, NULL, NULL, D_actor_535700_8013B72C, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013BA0C[2] = {
#include "assets/actor_535700_animation_09E1C_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013BA24[30] = {
#include "assets/actor_535700_animation_09E1C_bank4.inc"
};

GpAnimRec D_actor_535700_8013BA9C[94] = {
#include "assets/actor_535700_animation_09E1C_records.inc"
};

u16 D_actor_535700_8013BC14[20] = {
#include "assets/actor_535700_animation_09E1C_indices.inc"
};

GpAnimSet D_actor_535700_8013BC3C = {
    D_actor_535700_8013BA9C,
    D_actor_535700_8013BC14,
    { NULL, D_actor_535700_8013BA0C, NULL, NULL, D_actor_535700_8013BA24, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013BC64[2] = {
#include "assets/actor_535700_animation_0A0AC_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013BC7C[48] = {
#include "assets/actor_535700_animation_0A0AC_bank4.inc"
};

GpAnimRec D_actor_535700_8013BD3C[90] = {
#include "assets/actor_535700_animation_0A0AC_records.inc"
};

u16 D_actor_535700_8013BEA4[20] = {
#include "assets/actor_535700_animation_0A0AC_indices.inc"
};

GpAnimSet D_actor_535700_8013BECC = {
    D_actor_535700_8013BD3C,
    D_actor_535700_8013BEA4,
    { NULL, D_actor_535700_8013BC64, NULL, NULL, D_actor_535700_8013BC7C, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013BEF4[5] = {
#include "assets/actor_535700_animation_0A494_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013BF30[59] = {
#include "assets/actor_535700_animation_0A494_bank4.inc"
};

GpAnimRec D_actor_535700_8013C01C[156] = {
#include "assets/actor_535700_animation_0A494_records.inc"
};

u16 D_actor_535700_8013C28C[20] = {
#include "assets/actor_535700_animation_0A494_indices.inc"
};

GpAnimSet D_actor_535700_8013C2B4 = {
    D_actor_535700_8013C01C,
    D_actor_535700_8013C28C,
    { NULL, D_actor_535700_8013BEF4, NULL, NULL, D_actor_535700_8013BF30, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013C2DC[3] = {
#include "assets/actor_535700_animation_0A744_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013C300[29] = {
#include "assets/actor_535700_animation_0A744_bank4.inc"
};

GpAnimRec D_actor_535700_8013C374[114] = {
#include "assets/actor_535700_animation_0A744_records.inc"
};

u16 D_actor_535700_8013C53C[20] = {
#include "assets/actor_535700_animation_0A744_indices.inc"
};

GpAnimSet D_actor_535700_8013C564 = {
    D_actor_535700_8013C374,
    D_actor_535700_8013C53C,
    { NULL, D_actor_535700_8013C2DC, NULL, NULL, D_actor_535700_8013C300, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013C58C[2] = {
#include "assets/actor_535700_animation_0A900_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013C5A4[20] = {
#include "assets/actor_535700_animation_0A900_bank4.inc"
};

GpAnimRec D_actor_535700_8013C5F4[65] = {
#include "assets/actor_535700_animation_0A900_records.inc"
};

u16 D_actor_535700_8013C6F8[20] = {
#include "assets/actor_535700_animation_0A900_indices.inc"
};

GpAnimSet D_actor_535700_8013C720 = {
    D_actor_535700_8013C5F4,
    D_actor_535700_8013C6F8,
    { NULL, D_actor_535700_8013C58C, NULL, NULL, D_actor_535700_8013C5A4, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013C748[3] = {
#include "assets/actor_535700_animation_0ACE8_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013C76C[65] = {
#include "assets/actor_535700_animation_0ACE8_bank4.inc"
};

GpAnimRec D_actor_535700_8013C870[156] = {
#include "assets/actor_535700_animation_0ACE8_records.inc"
};

u16 D_actor_535700_8013CAE0[20] = {
#include "assets/actor_535700_animation_0ACE8_indices.inc"
};

GpAnimSet D_actor_535700_8013CB08 = {
    D_actor_535700_8013C870,
    D_actor_535700_8013CAE0,
    { NULL, D_actor_535700_8013C748, NULL, NULL, D_actor_535700_8013C76C, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013CB30[18] = {
#include "assets/actor_535700_animation_0B698_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013CC08[232] = {
#include "assets/actor_535700_animation_0B698_bank4.inc"
};

GpAnimRec D_actor_535700_8013CFA8[314] = {
#include "assets/actor_535700_animation_0B698_records.inc"
};

u16 D_actor_535700_8013D490[20] = {
#include "assets/actor_535700_animation_0B698_indices.inc"
};

GpAnimSet D_actor_535700_8013D4B8 = {
    D_actor_535700_8013CFA8,
    D_actor_535700_8013D490,
    { NULL, D_actor_535700_8013CB30, NULL, NULL, D_actor_535700_8013CC08, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013D4E0[4] = {
#include "assets/actor_535700_animation_0BA14_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013D510[68] = {
#include "assets/actor_535700_animation_0BA14_bank4.inc"
};

GpAnimRec D_actor_535700_8013D620[123] = {
#include "assets/actor_535700_animation_0BA14_records.inc"
};

u16 D_actor_535700_8013D80C[20] = {
#include "assets/actor_535700_animation_0BA14_indices.inc"
};

GpAnimSet D_actor_535700_8013D834 = {
    D_actor_535700_8013D620,
    D_actor_535700_8013D80C,
    { NULL, D_actor_535700_8013D4E0, NULL, NULL, D_actor_535700_8013D510, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_8013D85C[2] = {
#include "assets/actor_535700_animation_0BC60_bank1.inc"
};

AnimationPackedRotation D_actor_535700_8013D874[27] = {
#include "assets/actor_535700_animation_0BC60_bank4.inc"
};

GpAnimRec D_actor_535700_8013D8E0[94] = {
#include "assets/actor_535700_animation_0BC60_records.inc"
};

u16 D_actor_535700_8013DA58[20] = {
#include "assets/actor_535700_animation_0BC60_indices.inc"
};

GpAnimSet D_actor_535700_8013DA80 = {
    D_actor_535700_8013D8E0,
    D_actor_535700_8013DA58,
    { NULL, D_actor_535700_8013D85C, NULL, NULL, D_actor_535700_8013D874, NULL, NULL, NULL },
};

s16 D_actor_535700_8013DAA8 = 8;

Actor535700MsgEntry D_actor_535700_8013DAAC[6] = {
    { 2003, { .call2 = func_actor_535700_801327BC } },
    { 2005, { .call7 = func_actor_535700_8013284C } },
    { 2004, { .call4 = func_actor_535700_80132894 } },
    { 2011, { .call3 = func_actor_535700_80132910 } },
    { 2013, { .call6 = func_actor_535700_80132960 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_535700_8013DADC = { 1, 192, func_actor_535700_80132478, { .model = &D_actor_535700_80139A6C } };

u8 D_actor_535700_8013DAE8[140] = {
    0,
    0,
    0,
    0,
    192,
    174,
    19,
    128,
    200,
    178,
    19,
    128,
    212,
    182,
    19,
    128,
    228,
    185,
    19,
    128,
    60,
    188,
    19,
    128,
    204,
    190,
    19,
    128,
    180,
    194,
    19,
    128,
    100,
    197,
    19,
    128,
    0,
    0,
    0,
    0,
    32,
    199,
    19,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    124,
    156,
    19,
    128,
    116,
    160,
    19,
    128,
    88,
    172,
    19,
    128,
    8,
    203,
    19,
    128,
    184,
    212,
    19,
    128,
    128,
    218,
    19,
    128,
    52,
    216,
    19,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

TmdBone D_actor_535700_8013DB74[19] = {
#include "assets/actor_535700_model_11038_skeleton.inc"
};

u32 D_actor_535700_8013DE20[19] = {
#include "assets/actor_535700_model_11038_partVerts.inc"
};

SVECTOR D_actor_535700_8013DE6C[339] = {
#include "assets/actor_535700_model_11038_verts.inc"
};

SVECTOR D_actor_535700_8013E904[346] = {
#include "assets/actor_535700_model_11038_normals.inc"
};

u32 D_actor_535700_8013F3D4[3745] = {
#include "assets/actor_535700_model_11038_stream.inc"
};

TmdSource D_actor_535700_80142E58 = {
    0,
    20476,
    5672,
    19,
    D_actor_535700_8013DE20,
    D_actor_535700_8013DE6C,
    D_actor_535700_8013E904,
    D_actor_535700_8013DB74,
    D_actor_535700_8013F3D4,
};

TmdBone D_actor_535700_80142E7C[1] = {
#include "assets/actor_535700_model_1157C_skeleton.inc"
};

u32 D_actor_535700_80142EA0[1] = {
#include "assets/actor_535700_model_1157C_partVerts.inc"
};

SVECTOR D_actor_535700_80142EA4[29] = {
#include "assets/actor_535700_model_1157C_verts.inc"
};

SVECTOR D_actor_535700_80142F8C[24] = {
#include "assets/actor_535700_model_1157C_normals.inc"
};

u32 D_actor_535700_8014304C[212] = {
#include "assets/actor_535700_model_1157C_stream.inc"
};

TmdSource D_actor_535700_8014339C = {
    0,
    1436,
    0,
    1,
    D_actor_535700_80142EA0,
    D_actor_535700_80142EA4,
    D_actor_535700_80142F8C,
    D_actor_535700_80142E7C,
    D_actor_535700_8014304C,
};

GpPackedPose D_actor_535700_801433C0[21] = {
#include "assets/actor_535700_animation_121B0_bank1.inc"
};

AnimationPackedRotation D_actor_535700_801434BC[317] = {
#include "assets/actor_535700_animation_121B0_bank4.inc"
};

GpAnimRec D_actor_535700_801439B0[382] = {
#include "assets/actor_535700_animation_121B0_records.inc"
};

u16 D_actor_535700_80143FA8[20] = {
#include "assets/actor_535700_animation_121B0_indices.inc"
};

GpAnimSet D_actor_535700_80143FD0 = {
    D_actor_535700_801439B0,
    D_actor_535700_80143FA8,
    { NULL, D_actor_535700_801433C0, NULL, NULL, D_actor_535700_801434BC, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_80143FF8[12] = {
#include "assets/actor_535700_animation_12918_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80144088[187] = {
#include "assets/actor_535700_animation_12918_bank4.inc"
};

GpAnimRec D_actor_535700_80144374[231] = {
#include "assets/actor_535700_animation_12918_records.inc"
};

u16 D_actor_535700_80144710[20] = {
#include "assets/actor_535700_animation_12918_indices.inc"
};

GpAnimSet D_actor_535700_80144738 = {
    D_actor_535700_80144374,
    D_actor_535700_80144710,
    { NULL, D_actor_535700_80143FF8, NULL, NULL, D_actor_535700_80144088, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_80144760[20] = {
#include "assets/actor_535700_animation_13508_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80144850[321] = {
#include "assets/actor_535700_animation_13508_bank4.inc"
};

GpAnimRec D_actor_535700_80144D54[363] = {
#include "assets/actor_535700_animation_13508_records.inc"
};

u16 D_actor_535700_80145300[20] = {
#include "assets/actor_535700_animation_13508_indices.inc"
};

GpAnimSet D_actor_535700_80145328 = {
    D_actor_535700_80144D54,
    D_actor_535700_80145300,
    { NULL, D_actor_535700_80144760, NULL, NULL, D_actor_535700_80144850, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_80145350[16] = {
#include "assets/actor_535700_animation_13E70_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80145410[237] = {
#include "assets/actor_535700_animation_13E70_bank4.inc"
};

GpAnimRec D_actor_535700_801457C4[297] = {
#include "assets/actor_535700_animation_13E70_records.inc"
};

u16 D_actor_535700_80145C68[20] = {
#include "assets/actor_535700_animation_13E70_indices.inc"
};

GpAnimSet D_actor_535700_80145C90 = {
    D_actor_535700_801457C4,
    D_actor_535700_80145C68,
    { NULL, D_actor_535700_80145350, NULL, NULL, D_actor_535700_80145410, NULL, NULL, NULL },
};

GpPackedPose D_actor_535700_80145CB8[26] = {
#include "assets/actor_535700_animation_14998_bank1.inc"
};

AnimationPackedRotation D_actor_535700_80145DF0[224] = {
#include "assets/actor_535700_animation_14998_bank4.inc"
};

GpAnimRec D_actor_535700_80146170[392] = {
#include "assets/actor_535700_animation_14998_records.inc"
};

u16 D_actor_535700_80146790[20] = {
#include "assets/actor_535700_animation_14998_indices.inc"
};

GpAnimSet D_actor_535700_801467B8 = {
    D_actor_535700_80146170,
    D_actor_535700_80146790,
    { NULL, D_actor_535700_80145CB8, NULL, NULL, D_actor_535700_80145DF0, NULL, NULL, NULL },
};

Actor535700MsgEntry D_actor_535700_801467E0[6] = {
    { 2003, { .call1 = func_actor_535700_801331E4 } },
    { 2005, { .call7 = func_actor_535700_80133250 } },
    { 2004, { .call4 = func_actor_535700_801332B4 } },
    { 2011, { .call0 = func_actor_535700_8013332C } },
    { 2013, { .call5 = func_actor_535700_80133334 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_535700_80146810[2] = {
    { 257, 96, func_actor_535700_80132F20, { .model = &D_actor_535700_80142E58 } },
    { 257, 96, func_actor_535700_801333FC, { .model = &D_actor_535700_8014339C } },
};

u8 D_actor_535700_80146828[24] = {
    0,
    0,
    0,
    0,
    208,
    63,
    20,
    128,
    56,
    71,
    20,
    128,
    40,
    83,
    20,
    128,
    144,
    92,
    20,
    128,
    184,
    103,
    20,
    128,
};

s32 D_actor_535700_80146840;

Actor151000Work* D_actor_535700_80146844;

Task* D_actor_535700_80146848;

s16 D_actor_535700_8014684C;

static void func_actor_535700_80131FA0(GpEnemy* enemy, Task* task);
static void func_actor_535700_80132B58(GpEnemy* enemy, Task* task);

/// The fade task: while `D_actor_535700_80146840` is non-zero, draws a
/// full-screen black `TILE` into ordering table slot 0xA; once it reaches zero
/// the task kills itself. The count drops by one every frame.
void func_actor_535700_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_535700_80146840 != 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        SetTile(tile);
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        addPrim(gGpuCurrentOt + 0xA, tile);
    } else {
        taskKill(task);
    }
    D_actor_535700_80146840--;
}

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_535700_80131EF0(s32 frames)
{
    D_actor_535700_80146840 = frames;
    if (frames != 0) {
        Task_SpawnFromTable(&D_actor_535700_8013346C, 0, 0, 0);
    }
}

void func_actor_535700_80131F2C(void)
{
    if (Mc_SaveData[0].state.demoScene != 9) {
        Mc_SaveData[0].state.at4.loc.area = 0x1D;
        Mc_SaveData[0].state.at4.loc.warp = 5;
        Mc_SaveData[0].state.at4.loc.room = 2;
        gDisplayState.roomVariant         = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Mc_SaveData[0].state.sceneEvent = 6;
        Gp_RestoreStreamRng();
    }
}

/// State 0 of the first enemy's task: allocates the work block, publishes it
/// in `D_actor_535700_80146844` and on the task's work slot, points the
/// model's light and colour matrices and its animation context at it,
/// publishes the task in `D_actor_535700_80146848`, then runs the runner once
/// and advances the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
static void func_actor_535700_80131FA0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor151000Work* work;
    TmdObject*       obj;
    GpCoord*         coord;

    obj                     = task->extra.tmd;
    coord                   = obj->coords;
    work                    = memCalloc(0x4C0, 0);
    D_actor_535700_80146844 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_535700_80132558;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_535700_80146844->light;
    obj->colorMtx                = &D_actor_535700_80146844->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_535700_80146848      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_535700_80146844->rig.anim, D_actor_535700_8013DAE8, obj,
                  &D_actor_535700_80146844->rig.poses, D_actor_535700_80146844->rig.slots);
    D_actor_535700_80146844->st.animId  = 1;
    D_actor_535700_80146844->st.state   = 2;
    D_actor_535700_80146844->st.travel  = 0;
    D_actor_535700_80146844->turnFrames = 0;
    D_actor_535700_80146844->stepRec    = 0;
    D_actor_535700_80146844->footsteps  = 0;
    task->msgTable                      = D_actor_535700_8013DAAC;
    func_actor_535700_80132108(task);
    task->state += 1;
}

/// Per-frame update: states 1 and 2 run their one-shot animation reseed and
/// leave the work block in state 3; state 3 walks the model while `st.travel`
/// counts down (distance picked by `D_actor_535700_8014684C`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation and, once
/// `footsteps` is set, plays the footsteps.
static void func_actor_535700_80132108(Task* task)
{
    GpCoord*         coord = task->extra.tmd->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_535700_80146844->st.state == 1) {
        func_actor_535700_80132730();
        D_actor_535700_80146844->st.state = 3;
    } else if (D_actor_535700_80146844->st.state == 2) {
        func_actor_535700_80132694();
        D_actor_535700_80146844->st.state = 3;
    } else if (D_actor_535700_80146844->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_535700_8014684C) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state          = 1;
                    D_actor_535700_8013DAA8 = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_535700_80132648();
        if (work->footsteps != 0) {
            func_actor_535700_80132580(task);
        }
    }
}

/// The first enemy's task body: publishes the task's work block in
/// `D_actor_535700_80146844`, then runs the handler for the task's state from
/// a table built on the stack - the spawn handler `func_actor_535700_80131FA0`,
/// then the per-frame `func_actor_535700_801324D4`.
void func_actor_535700_80132478(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_535700_80131FA0,
        func_actor_535700_801324D4,
    };

    D_actor_535700_80146844 = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 1 of the first enemy's task: refreshes the model root's coordinate,
/// hands `func_800D7A9C` the point 0x320 above it, then runs the runner and
/// draws the ground shadow.
static void func_actor_535700_801324D4(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_535700_80132108(task);
    func_actor_535700_80132ABC(task);
}

/// Exit callback the first enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
static void func_actor_535700_80132558(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Plays a step sound whenever animation slot 1 rolls onto a new record whose
/// flags nibble is 0x10 or 0x20 - the two feet - panned and attenuated from
/// the second coordinate of the task's model. The record is latched in
/// `stepRec` so each one fires once.
static void func_actor_535700_80132580(Task* task)
{
    Actor151000Work* work;
    GpCoord*         obj;
    GpAnimRec*       rec;
    s32              kind;
    s32              id;
    s32              pan;

    work = (Actor151000Work*)task->work;
    obj  = task->extra.tmd->coords + 1;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec == NULL || rec == work->stepRec) {
        return;
    }
    work->stepRec = rec;
    kind          = rec->flags & 0x30;
    if (kind != 0x10 && kind != 0x20) {
        return;
    }
    id = 0x1000000F;
    if (kind == 0x10) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
}

/// Ticks animation slots 1..0x12 of the first enemy's animation context.
static void func_actor_535700_80132648(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_535700_80146844->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a reset
/// argument, and latches the clip into `st.appliedAnimId`. Clears the footstep
/// check's record first.
static void func_actor_535700_80132694(void)
{
    s32 i;

    D_actor_535700_80146844->stepRec = NULL;
    i                                = 1;
    do {
        D_actor_535700_80146844->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_535700_80146844->rig.anim, i, D_actor_535700_80146844->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_535700_80146844->st.appliedAnimId = D_actor_535700_80146844->st.animId;
}

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `D_actor_535700_8013DAA8` as the reset argument, and latches the clip into
/// `st.appliedAnimId`. Clears the footstep check's record first.
static void func_actor_535700_80132730(void)
{
    s32 i;

    D_actor_535700_80146844->stepRec = 0;
    i                                = 1;
    do {
        func_800B4114(&D_actor_535700_80146844->rig.anim, i, D_actor_535700_80146844->st.animId, 0,
                      D_actor_535700_8013DAA8);
        i++;
    } while (i < 0x13);
    D_actor_535700_80146844->st.appliedAnimId = D_actor_535700_80146844->st.animId;
}

/// "Start animation" opcode of the first enemy: `withArg` selects between the
/// two start paths the runner `func_actor_535700_80132108` dispatches on, and
/// only the first carries `animArg`, which it leaves in
/// `D_actor_535700_8013DAA8`. Returns -1, without touching the work block, when
/// the clip id is 0x23 or more.
s32 func_actor_535700_801327BC(Task* task, s32 arg1, GpAnimArg* args, s32 arg3)
{
    if (args->field_4 < 0x23) {
        D_actor_535700_80146844->st.animId = args->field_4;
        if (args->field_8 != 0) {
            D_actor_535700_80146844->st.state = 1;
            D_actor_535700_8013DAA8           = args->field_C;
        } else {
            D_actor_535700_80146844->st.state = 2;
        }
        D_actor_535700_80146844->st.field_6 = 0;
        func_actor_535700_80132108(D_actor_535700_80146848);
        return 0;
    }
    return -1;
}

/// Visibility opcode of the first enemy: applies `arg2` to the model of the
/// task published in `D_actor_535700_80146848` - bit 0 shows it (flags 0)
/// rather than hiding it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_535700_8013284C(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_535700_80146848->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}

/// Placement opcode of the first enemy: yaws the model's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block's `st.yaw`, then
/// drops the placement translation into the matrix and marks it dirty.
s32 func_actor_535700_80132894(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    u16      yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_535700_80146844->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler of the first enemy: message 0 arms the turn countdown
/// `turnFrames` at 0x14 frames, message 1 sets `footsteps`, which turns the
/// footsteps on. Anything else does nothing.
s32 func_actor_535700_80132910(Task* task, s32 arg1, GpCmdArg* msg)
{
    s32 kind;

    kind = msg->command;
    switch (kind) {
        case 0:
            D_actor_535700_80146844->turnFrames = 0x14;
            break;
        case 1:
            D_actor_535700_80146844->footsteps = kind;
            break;
    }
    return 0;
}

/// "Walk to" opcode of the first enemy: turns the model to face `target` --
/// away from it in mode 1 -- and leaves in `st.travel` the planar distance
/// divided by the walk's frame count: 60 in mode 0, 15 in mode 1 and 25 in
/// mode 2. The mode is kept in `D_actor_535700_8014684C`, which picks the
/// runner's step.
s32 func_actor_535700_80132960(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GpCoord*         coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_535700_8014684C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_535700_8014684C == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_535700_8014684C) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

/// Draws the first enemy's ground shadow quad under its model root, unless the
/// model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's shade is
/// the room's current `Gp_State1C` level.
static void func_actor_535700_80132ABC(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// State 0 of the second enemy's task. Allocates its `Actor150400Work`,
/// spawns its sub-model task from `D_actor_535700_80146810`, takes the
/// sub-model's texture page and CLUT from the placement record the enemy's
/// `placeKey` selects, makes the sub-model a child of this task, lights the
/// model at its world position, starts the animation and runs the state
/// machine `func_actor_535700_80132D68` once.
static void func_actor_535700_80132B58(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor150400Work* work;
    GpCoord*         coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = work = (Actor150400Work*)memCalloc(sizeof(Actor150400Work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_535700_80132FF8;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_535700_80146810, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_535700_80146828, obj,
                  &work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_535700_801467E0;
    func_actor_535700_80132D68(task);
    task->state++;
}

/// The second enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `func_actor_535700_80133180` or `func_actor_535700_80133108` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
static void func_actor_535700_80132D68(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_535700_80133180(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_535700_80133108(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_535700_801330BC(task);
        return;
    }
}

/// The second enemy's task body: runs the handler for the task's state from a
/// table built on the stack - the spawn handler `func_actor_535700_80132B58`,
/// then the per-frame `func_actor_535700_80132F74`.
void func_actor_535700_80132F20(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_535700_80132B58,
        func_actor_535700_80132F74,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State 1 of the second enemy's task: refreshes the model root's coordinate,
/// hands `func_800D7A9C` the point 0x320 above it, then runs the state machine
/// and draws the ground shadow.
static void func_actor_535700_80132F74(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_535700_80132D68(task);
    func_actor_535700_80133020(task);
}

/// Exit callback the second enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
static void func_actor_535700_80132FF8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the second enemy's ground shadow quad under its model root, unless
/// the model is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3`, and the quad's shade is
/// the room's current `Gp_State1C` level.
static void func_actor_535700_80133020(Task* task)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks the second enemy's animation slots 1..0x12.
static void func_actor_535700_801330BC(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets the second enemy's animation slots 1..0x12 to clip `animId` at rate
/// 1, without a reset argument, and latches the clip into `appliedAnimId`.
static void func_actor_535700_80133108(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the second enemy's animation slots 1..0x12 on clip `animId`,
/// forwarding `animArg` as the reset argument, and latches the clip into
/// `appliedAnimId`.
static void func_actor_535700_80133180(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->st.appliedAnimId = work->st.animId;
}

/// "Start animation" opcode of the second enemy: `withArg` selects between the
/// two start paths `func_actor_535700_80132D68` dispatches on, and only the
/// first carries `animArg`. Returns -1, without touching the work block, when
/// the clip id is 6 or more.
s32 func_actor_535700_801331E4(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->field_4 < 6) {
        work->st.animId = args->field_4;
        if (args->field_8 != 0) {
            work->st.state = 1;
            work->animArg  = args->field_C;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_535700_80132D68(task);
        return 0;
    }
    return -1;
}

/// Visibility opcode of the second enemy: sets `TmdObject::flags` on its own
/// model and on the sub-model task's in `pairTask` at once. `flags` bit 0
/// shows them (0) rather than hiding them (0x80), and bit 1 ORs 4 in.
s32 func_actor_535700_80133250(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor150400Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Placement opcode of the second enemy: yaws its root coordinate to
/// `placement->rot.vy`, caching that yaw in `Actor150400Work::yaw`, then
/// drops the placement translation into the matrix and marks it dirty.
s32 func_actor_535700_801332B4(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*         coord;
    Actor150400Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_535700_8013332C(void)
{
    return 0;
}

/// "Walk to" opcode of the second enemy: turns its root coordinate to face
/// `target`, caching the yaw in `Actor150400Work::yaw`, and leaves the
/// horizontal distance to it, in twelfths, in `travel` for the walk state to
/// count down.
s32 func_actor_535700_80133334(Task* task, s32 arg1, VECTOR* target)
{
    GpCoord*         coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Task body of the second enemy's sub-model, which the enemy's spawn handler
/// makes a child of the enemy's task. On its first tick it lights the
/// sub-model with the enemy's `Actor150400Work` matrices and hangs its
/// root coordinate off part 7 of the enemy's model; after that it only marks
/// the coordinate dirty each frame so it follows that part.
void func_actor_535700_801333FC(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GpCoord*         coord  = obj->coords;
    GpCoord*         sub    = &parent->extra.tmd->coords[7];
    Actor150400Work* work   = (Actor150400Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
