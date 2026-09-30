#include "mist_parking_private.h"

#include "types.h"

#include "rooms/mist_parking.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/starter_inventory.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/room_common.h"

void func_mist_parking_8018345C(Task* arg0);

extern GpEvsCmd D_mist_parking_8018F374[];
extern GpEvsCmd D_mist_parking_8018F4AC[];
extern GpEvsCmd D_mist_parking_8018F5E4[];
extern GpEvsCmd D_mist_parking_8018F824[];
extern GpEvsCmd D_mist_parking_8018F9A4[];
extern GpEvsCmd D_mist_parking_8018FA4C[];
extern GpEvsCmd D_mist_parking_8018FB3C[];
extern s32      D_mist_parking_8018FBFC[];
extern s32      D_mist_parking_8018FC10[];

static void func_mist_parking_801833F8(Task* task);

static void func_mist_parking_8018307C(Task* task);
static void func_mist_parking_801830F8(Task* task);
static void func_mist_parking_80183304(Task* task);
static void func_mist_parking_80183434(Task* arg0);

/// State handlers of the same shape for a task that attaches a model to a
/// parent's part and then idles; nothing in the room reads this table.
static const TaskFuncTable3 D_mist_parking_8017D7E8 = {
    {
        func_mist_parking_8018307C,
        func_mist_parking_801830F8,
        taskKill,
    },
};
/// State handlers of the text-block task `func_mist_parking_801832AC` runs.
static const TaskFuncTable3 D_mist_parking_8017D7F4 = {
    {
        func_mist_parking_80183304,
        func_mist_parking_801833F8,
        func_mist_parking_80183434,
    },
};

extern AnimationPlayRequest D_mist_parking_8018D848;
extern AnimationPlayRequest D_mist_parking_8018DE38;
extern AnimationPlayRequest D_mist_parking_8018DE88;
extern AnimationPlayRequest D_mist_parking_8018DEB0;
extern AnimationPlayRequest D_mist_parking_8018DEC4;
extern AnimationPlayRequest D_mist_parking_8018DED8;

void func_mist_parking_80182A44(Task*);
void func_mist_parking_80182F60(Task*);
void func_mist_parking_80183100(s32);
void func_mist_parking_8018312C(s32);
void func_mist_parking_8018316C(s32);
void func_mist_parking_801831F0(s32);
void func_mist_parking_8018326C(s32);
void func_mist_parking_801832AC(Task*);
void func_mist_parking_801834D4(Task*);
void func_mist_parking_8018354C(void);
void func_mist_parking_8018357C(Task*);
void func_mist_parking_80183600(void);

TaskDesc D_mist_parking_8018D75C[9] = {
    { 1, 192, func_mist_parking_80183B40, { .model = &D_mist_parking_80187294 } },
    { 0, 192, func_mist_parking_801832AC, { .model = NULL } },
    { 0, 192, func_mist_parking_8018345C, { .model = NULL } },
    { 0, 192, func_mist_parking_8018357C, { .model = NULL } },
    { 0, 97, func_mist_parking_801828F0, { .model = NULL } },
    { 0, 192, func_mist_parking_801836CC, { .model = NULL } },
    { 0, 192, func_mist_parking_801834D4, { .model = NULL } },
    { 0, 192, func_mist_parking_80182A44, { .model = NULL } },
    { 0, 192, func_mist_parking_80182F60, { .model = NULL } },
};

AnimationSet* D_mist_parking_8018D7C8[25] = {
    NULL,
    &D_mist_parking_80187594,
    &D_mist_parking_80187D34,
    &D_mist_parking_8018821C,
    &D_mist_parking_801886F8,
    &D_mist_parking_80188CC0,
    &D_mist_parking_80189074,
    &D_mist_parking_80189770,
    &D_mist_parking_80189C48,
    &D_mist_parking_8018A340,
    &D_mist_parking_8018A620,
    &D_mist_parking_8018AC64,
    &D_mist_parking_8018B054,
    &D_mist_parking_8018B454,
    &D_mist_parking_8018B790,
    &D_mist_parking_8018BCA0,
    &D_mist_parking_8018BFCC,
    &D_mist_parking_8018C3A0,
    &D_mist_parking_8018C70C,
    &D_mist_parking_8018CB34,
    &D_mist_parking_8018CDB0,
    &D_mist_parking_8018D220,
    &D_mist_parking_8018D41C,
    &D_mist_parking_8018D734,
    &D_mist_parking_8018A9A4,
};

GpCopyArg D_mist_parking_8018D82C = { { .sets = D_mist_parking_8018D7C8 }, 25 };

AnimationPlayRequest D_mist_parking_8018D834 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D848 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D85C = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D870 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D884 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D898 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8AC = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8C0 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8D4 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8E8 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8FC = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D910 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D924 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D938 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D94C = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D960 = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D974 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D988 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D99C = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9B0 = { { .index = 1 }, 66, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9C4 = { { .index = 1 }, 67, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9D8 = { { .index = 1 }, 68, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9EC = { { .index = 1 }, 69, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DA00 = { { .index = 1 }, 70, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DA14 = { { .index = 1 }, 71, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

s8 D_mist_parking_8018DA28[28] = {
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
};

ActorTransform D_mist_parking_8018DA44 = { { 3310, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DA5C = { { 4494, 0, -3867, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_mist_parking_8018DA74 = { { 3270, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DA8C = { { 4370, 0, -4000, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_mist_parking_8018DAA4 = { { 4560, 0, -4916, 0 }, { 0, 1630, 0, 0 } };

AnimationPlayRequest D_mist_parking_8018DABC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DAD0 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DAE4[3] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DB20 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB34 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB48 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB5C = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB70[6] = {
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DBE8 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DBFC = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC10 = { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC24 = { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC38 = { { .index = 0 }, 19, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC4C = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC60[6] = {
    { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DCD8 = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DCEC = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD00 = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD14 = { { .index = 0 }, 30, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD28 = { { .index = 0 }, 31, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD3C[2] = {
    { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 33, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DD64 = { { .index = 0 }, 34, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD78 = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_mist_parking_8018DD8C = { { 6400, 0, -4800, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DDA4 = { { 5150, 0, -4450, 0 }, { 0, -515, 0, 0 } };

ActorTransform D_mist_parking_8018DDBC = { { 5030, 0, -5680, 0 }, { 0, -296, 0, 0 } };

ActorTransform D_mist_parking_8018DDD4 = { { -1960, 0, -4650, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DDEC = { { 5150, 0, -4450, 0 }, { 0, -715, 0, 0 } };

GpSpawnAnimArg D_mist_parking_8018DE04 = { 2, 26 };

GpSpawnAnimArg D_mist_parking_8018DE0C = { 2, 33 };

ActorCommand D_mist_parking_8018DE14 = { { .loc = { 0, 0 } }, 0 };

ActorCommand D_mist_parking_8018DE18 = { { .loc = { 0, 0 } }, 1 };

ActorCommand D_mist_parking_8018DE1C = { { .loc = { 0, 0 } }, 2 };

ActorCommand D_mist_parking_8018DE20 = { { .loc = { 0, 0 } }, 3 };

AnimationPlayRequest D_mist_parking_8018DE24 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DE38 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DE4C[3] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DE88 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DE9C = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DEB0 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DEC4 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DED8 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_mist_parking_8018DEEC = { { 8460, 0, 92, 0 }, { 0, 2047, 0, 0 } };

u8 D_mist_parking_8018DF04[8] = {
    130,
    220,
    130,
    190,
    130,
    230,
    0,
    0,
};

u8 D_mist_parking_8018DF0C[8] = {
    130,
    166,
    130,
    166,
    0,
    0,
    0,
    0,
};

u8 D_mist_parking_8018DF14[8] = {
    130,
    205,
    130,
    162,
    0,
    0,
    0,
    0,
};

u8 D_mist_parking_8018DF1C[8] = {
    130,
    162,
    130,
    162,
    130,
    166,
    0,
    0,
};

u8* D_mist_parking_8018DF24[4] = {
    D_mist_parking_8018DF04,
    D_mist_parking_8018DF0C,
    D_mist_parking_8018DF14,
    D_mist_parking_8018DF1C,
};

GpEvsCmd D_mist_parking_8018DF34[155] = {
    { 32, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA44 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DD8C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DEEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE14 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DE88 }, { .value = 0 } },
    { 3, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_801831F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D884 }, { .value = 0 } },
    { 15, { .value = 0x51130005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_8018326C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA74 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D898 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_mist_parking_8018DDA4 }, { .storage = &D_mist_parking_8018DE0C } },
    { 4, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8AC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8C0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DCEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DDA4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5113000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 16, { .value = 0x5113000C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8D4 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5113000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DCD8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DD64 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8E8 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130011 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA5C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D924 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DC10 }, { .value = 0 } },
    { 4, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x51130008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA5C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DC24 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8FC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE1C } }, { .value = 0 } },
    { 4, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x5113000E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE18 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DC4C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D924 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9B0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DCD8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D870 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DD00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DD28 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9B0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DCD8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D938 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D94C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DD64 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA8C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DC38 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D924 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DDEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DB34 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA5C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018DA00 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DDA4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018DA14 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DB34 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D974 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183BAC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_mist_parking_8018DDBC }, { .storage = &D_mist_parking_8018DE04 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183780 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018EDBC[23] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_8018326C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183BAC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DA5C }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DDA4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE18 } }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_mist_parking_8018DDBC }, { .storage = &D_mist_parking_8018DE04 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183780 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018EFE4[8] = {
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DDA4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE18 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_mist_parking_8018DDBC }, { .storage = &D_mist_parking_8018DE04 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_mist_parking_8018DEEC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DE88 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F0A4[10] = {
    { 13, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183100 }, { .value = 0xF0000 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DB5C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F194[10] = {
    { 13, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183100 }, { .value = 0xF0001 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DB5C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F284[10] = {
    { 13, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183100 }, { .value = 0xF0002 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DB5C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F374[13] = {
    { 13, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE1C } }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183708 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_mist_parking_8018DAA4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DB20 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F4AC[13] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183100 }, { .value = 0x50001 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9D8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DBE8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9D8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183708 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F5E4[24] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183100 }, { .value = 0x50002 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018DA00 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DBE8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_mist_parking_8018DA44 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_mist_parking_8018DDD4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183688 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183708 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_8018312C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_mist_parking_8018354C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F824[16] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183100 }, { .value = 0x50003 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9D8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DAD0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DBE8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183634 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 5, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183688 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_8018316C }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mist_parking_80183708 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018F9A4[7] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DE38 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DEB0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018FA4C[10] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 13, { .callbackResult = func_800D4D2C }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DEC4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DED8 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DE88 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mist_parking_8018FB3C[8] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mist_parking_8018D82C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DEC4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DED8 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_mist_parking_8018DE88 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_mist_parking_8018FBFC[5] = {
    161,
    61,
    63,
    11,
    108,
};

s32 D_mist_parking_8018FC10[5] = {
    50,
    1,
    1,
    1,
    1,
};

/// The two text lines of the block `func_mist_parking_80183304` shows, and
/// the alternative pair it uses when the task's `spawnArg1` is 1.
extern u8* D_mist_parking_8018DF24[4];

void func_mist_parking_80182A44(Task* task)
{
    s32                   i;
    s32                   flag;
    s16                   idx;
    MistParkingScanState* st = &D_mist_parking_80195328;

    switch (task->state) {
        case 0:
            Mem_Set(st, 0, 4);
            func_800E8614(D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(2, 0);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            for (i = 0; i < 5; i++) {
                if (GameFlag_GetNibble(i + 0x125) == 2) {
                    task->state = 2;
                    return;
                }
            }
            task->state = 6;
            break;
        case 2:
            func_mist_parking_80183708(2);
            func_800E8614(D_mist_parking_8018F9A4, 1);
            Gp_RunCapCmd(6, 0);
            task->state++;
            break;
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            switch (Gp_GetCapEventKey()) {
                case 1:
                    st->timer = 10;
                    Gp_RunCapCmd(7, 0);
                    task->state = 4;
                    break;
                case 4:
                    for (i = 0; i < 5; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2) {
                            GameFlag_SetNibble(flag, 3);
                        }
                    }
                    func_800E8614(D_mist_parking_8018F9A4, 1);
                    Gp_RunCapCmd(8, 0);
                    task->state = 6;
                    break;
                case 3:
                    for (i = 0; i < 4; i++) {
                        flag = i + 0x125;
                        if (GameFlag_GetNibble(flag) == 2 && Gp_GiveItem(Gp_ScanPtrs[3], D_mist_parking_8018FBFC[i], D_mist_parking_8018FC10[i]) != 0) {
                            GameFlag_SetNibble(flag, 3);
                            Gp_SetCurBit2Flag(i + 0x20, 2);
                        }
                    }
                    if (GameFlag_GetNibble(0x129) == 2 && func_800B7420(0x6C) == 0) {
                        if (Gp_GiveItem(D_8010D55C, 0x6C, 1) != 0) {
                            GameFlag_SetNibble(0x129, 3);
                            Gp_SetCurBit2Flag(0x24, 2);
                        }
                    }
                    func_800E8614(D_mist_parking_8018F9A4, 1);
                    Gp_RunCapCmd(4, 0);
                    task->state = 6;
                    break;
            }
            break;
        case 4:
            if (Gp_CapBusy() != 0) {
                return;
            }
            st->timer--;
            if (st->timer == 5) {
                idx = st->index;
                if (GameFlag_GetNibble(idx + 0x125) == 2) {
                    Gp_StartCapSlot(5, 0, idx);
                }
                return;
            }
            if (st->timer != 0) {
                return;
            }
            idx = st->index;
            if (Gp_GetCurBit2Flag(idx + 0x20) != 1) {
                GameFlag_SetNibble(idx + 0x125, 3);
            }
            st->timer = 10;
            st->index++;
            if (st->index >= 5) {
                task->state++;
            }
            break;
        case 5:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() == 0) {
                func_800E8614(D_mist_parking_8018F9A4, 1);
                Gp_RunCapCmd(2, 0);
                task->state++;
            }
            /* fallthrough */
        case 6:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case 7:
            task->killCountdown++;
            if (task->killCountdown >= 0xB) {
                func_mist_parking_80183708(0);
                Gp_RunCapCmd(4, 0);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 8:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 1) {
                func_800E8614(D_mist_parking_8018FA4C, 1);
            } else {
                func_800E8614(D_mist_parking_8018FB3C, 1);
            }
            task->state++;
            break;
        case 9:
            task->killCountdown++;
            if (task->killCountdown == 0xA) {
                Gp_RunCapCmd(3, 0);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 10:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

void func_mist_parking_80182F60(Task* task)
{
    s32 key;

    switch (task->state) {
        case 0:
            func_800E8614(D_mist_parking_8018F374, 1);
            task->state++;
            break;
        case 1:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 2:
            key                   = Gp_GetCapEventKey();
            task->spawnArg1.value = key;
            switch (key) {
                case 4:
                    func_800E8614(D_mist_parking_8018F4AC, 1);
                    break;
                case 5:
                    func_800E8614(D_mist_parking_8018F5E4, 1);
                    break;
                case 6:
                    func_800E8614(D_mist_parking_8018F824, 1);
                    break;
            }
            task->state++;
            break;
        case 4:
            if (task->spawnArg1.value == 4) {
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(task);
            break;
    }
}

/// Attaches the task's model to the coordinate frame of part `spawnArg1` of
/// the model of the task in `spawnArg2`, sharing its light and colour
/// matrices, reparents the task under that one and steps it on.
static void func_mist_parking_8018307C(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent              = (Task*)task->spawnArg2.pointer;
    part                = task->spawnArg1.value;
    extra               = task->extra.tmd;
    parentExtra         = parent->extra.tmd;
    coord               = extra->coords;
    dest                = &parentExtra->coords[part];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = dest;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// The empty per-frame state of `D_mist_parking_8017D7E8`.
static void func_mist_parking_801830F8(Task* task)
{
}

void func_mist_parking_80183100(s32 arg0)
{
    Gp_StartCapSlot(arg0 >> 16, 0, arg0);
}

void func_mist_parking_8018312C(s32 arg0)
{
    Task_SpawnFromTable(D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_8018316C(s32 arg0)
{
    Mc_SaveData[0].state.at4.loc.stage = 1;
    Mc_SaveData[0].state.at4.loc.warp  = 1;
    Mc_SaveData[0].state.at4.loc.room  = 1;
    Mc_SaveData[0].state.at4.loc.area  = arg0;
    gDisplayState.spriteVariant        = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        Fs_BeginBootLoad((u8*)&Mc_SaveData[0].state.at4.loc, 0);
    }
}

void func_mist_parking_801831F0(s32 arg0)
{
    Task**     slot;
    Task*      task;
    TmdObject* obj;

    if (arg0 == 0) {
        slot = &D_mist_parking_80195320;
    } else {
        slot = NULL;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = Task_SpawnFromTable(D_mist_parking_8018D75C, arg0, 0, 0);
        *slot = task;
        if (task != NULL) {
            obj         = task->extra.tmd;
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

void func_mist_parking_8018326C(s32 arg0)
{
    if (arg0 == 0) {
        if (D_mist_parking_80195320 != NULL) {
            taskKill(D_mist_parking_80195320);
        }
        D_mist_parking_80195320 = NULL;
    }
}

/// Runs the handler for the task's state from a stack copy of
/// `D_mist_parking_8017D7F4`: the text block's setup, its wait and its exit.
void func_mist_parking_801832AC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_parking_8017D7F4;
    sp.funcs[task->state](task);
}

/// Allocates a two-line text block, parks it at `Task::work`, spawns it and
/// steps the task on; `func_mist_parking_80183434` is set as the exit
/// callback.
static void func_mist_parking_80183304(Task* task)
{
    RoomTextBlock* block;
    TextLineNode*  node;
    u8**           line;
    s32            table;
    s32            off;
    s32            mode;
    s32            i;

    block = memCalloc(sizeof(RoomTextBlock), 0);
    node  = block->lines;
    if (block == NULL) {
        taskKill(task);
        return;
    }

    i                  = 0;
    mode               = 1;
    line               = D_mist_parking_8018DF24;
    table              = (s32)D_mist_parking_8018DF24;
    off                = 8;
    task->work         = block;
    task->exitCallback = func_mist_parking_80183434;

    for (; i < 2; i++) {
        if (task->spawnArg1.value == mode) {
            node->text = *(u8**)(off + table);
        } else {
            node->text = *line;
        }
        node->next = node + 1;
        node++;
        line++;
        off += 4;
    }
    node[-1].next = NULL;

    block->desc.count   = 2;
    block->desc.lines   = block->lines;
    block->desc.field_8 = 0;
    block->field_C      = 0;
    Ui_SpawnTextBlock(&block->desc, 0, 0, 0);
    task->state++;
}

/// Waits for the text block parked at `Task::work` to report a non-zero
/// `TextBlockDesc::field_2`, stores it through `Task::spawnArg2` and steps
/// the task on.
static void func_mist_parking_801833F8(Task* task)
{
    s16 result;

    result = ((RoomTextBlock*)task->work)->desc.field_2;
    if (result != 0) {
        *(s32*)task->spawnArg2.pointer = result;
        task->state                    = task->state + 1;
    }
}

/// Exit callback of the text-block task: kills it and calls
/// `Stage_SetEndingFlag`.
static void func_mist_parking_80183434(Task* arg0)
{
    taskKill(arg0);
    Stage_SetEndingFlag();
}

void func_mist_parking_8018345C(Task* arg0)
{
    if (gGameSession->eventState == 0 && Gp_CapBusy() == 0) {
        if (D_mist_parking_8019531C == 2) {
            func_800E8614(D_mist_parking_8018F5E4, 1);
        } else {
            func_800E8614(D_mist_parking_8018F4AC, 1);
        }
        taskKill(arg0);
    }
}

void func_mist_parking_801834D4(Task* arg0)
{
    if (gGameSession->eventState == 0 && Gp_CapBusy() == 0) {
        if (D_mist_parking_8019531C == 2) {
            func_800E8614(D_mist_parking_8018FB3C, 1);
        } else {
            func_800E8614(D_mist_parking_8018FA4C, 1);
        }
        taskKill(arg0);
    }
}

/// Spawns entry 3 of `D_mist_parking_8018D75C`.
void func_mist_parking_8018354C(void)
{
    Task_SpawnFromTable(D_mist_parking_8018D75C, 3, 0, 0);
}

void func_mist_parking_8018357C(Task* arg0)
{
    func_800BC4E4();
    Mc_SaveData[0].state.at4.loc.stage = 2;
    Mc_SaveData[0].state.at4.loc.area  = 1;
    Mc_SaveData[0].state.at4.loc.warp  = 1;
    Mc_SaveData[0].state.at4.loc.room  = 1;
    gDisplayState.spriteVariant        = 1;
    Fs_BeginBootLoad((u8*)&Mc_SaveData[0].state.at4.loc, 1);
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    taskKill(arg0);
}

/// Spawns entry 4 of `D_mist_parking_8018D75C` and keeps its handle in
/// `D_mist_parking_80195324`.
void func_mist_parking_80183600(void)
{
    D_mist_parking_80195324 = Task_SpawnFromTable(D_mist_parking_8018D75C, 4, 0, 0);
}
