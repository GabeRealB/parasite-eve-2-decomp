#include "rooms/acropolis_promenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
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
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

#define D_acropolis_promenade_80181AFC (D_acropolis_promenade_80181AF4 + 1)

/// Sign pair for one corner of the promenade's ground-glow quad
/// (`func_acropolis_promenade_8017ED44`). The four entries of
/// `D_acropolis_promenade_80181AE4` are the +/-1 combinations, scaled by 0x300
/// into the quad's `vx` / `vz`; the quad is flat, so there is no `y` component
/// to sign.
typedef struct ApmGlowCorner {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
} ApmGlowCorner;
STATIC_ASSERT_SIZEOF(ApmGlowCorner, 0x4);

/// Work block of the prop task, at `Task::work`. The task allocates it with
/// `memCalloc(4, 0)` and clears its one word; nothing else in the room reads it.
typedef struct {
    s32 field_0;
} ApmPropWork;
STATIC_ASSERT_SIZEOF(ApmPropWork, 0x4);

extern EvsCommand   D_acropolis_promenade_80180F00[];
extern EvsCommand   D_acropolis_promenade_80181068[];
extern s32          D_acropolis_promenade_80181140;
extern s32          D_acropolis_promenade_80181144;
extern RoomEventMsg D_acropolis_promenade_801862D0;
extern Task*        D_acropolis_promenade_801862D8;

/// The room's message table, installed on the room task by its setup state.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*, s32, TaskMessageArg, TaskMessageArg);
        s32  (*call1)(Task*, s32, RoomEventMsg*, RoomEventMsg*);
        s32  (*call2)(Task*, s32, s32, TaskMessageArg);
        void (*call3)(void);
    } handler;
} AcropolisPromenadeMsgEntry;
STATIC_ASSERT_SIZEOF(AcropolisPromenadeMsgEntry, 8);

extern AcropolisPromenadeMsgEntry D_acropolis_promenade_80180E74[];
/// Spawn table holding the prop task, terminated by an all-ones `flags`.
extern TaskDesc D_acropolis_promenade_80180EA4[];
/// The room's task table: the streamed-scene task is entry 2, and entries 3
/// and 4 are the fade tasks it spawns.
extern TaskDesc D_acropolis_promenade_80181148[];

/// Per-frame path the promenade's streamed scene walks the player's matrix
/// along, indexed backwards by `0x45 - gCdCmdQueue.movieFrame`, plus the script
/// pair the scene runs.
extern SVECTOR                   D_acropolis_promenade_80181184[];
extern PadScriptCmd              D_acropolis_promenade_80186224[6];
extern PadScriptVibrationSegment D_acropolis_promenade_8018623C[2];

extern ApmGlowCorner D_acropolis_promenade_80181AE4[];

extern u16 D_acropolis_promenade_80181B74;
extern u16 D_acropolis_promenade_80181B76;
extern u16 D_acropolis_promenade_80181B78[];

static void func_acropolis_promenade_8017D9E0(Task* arg0);
static void func_acropolis_promenade_8017DAA4(Task* task);
static void func_acropolis_promenade_8017DB48(Task* task);

void func_acropolis_promenade_8017DB9C(Task*);
void func_acropolis_promenade_8017DF74(Task*);
void func_acropolis_promenade_8017DFD4(Task*);

extern WorldCollisionGrid D_acropolis_promenade_801823DC[1];
extern WorldCollisionGrid D_acropolis_promenade_80182BD0[1];
extern GpObj4C            D_acropolis_promenade_80182BF4[6];
extern GpObj4C            D_acropolis_promenade_80182DBC[6];
extern GpRoomCoordSet     D_acropolis_promenade_80183A08[1];

extern AnimationPlayRequest D_acropolis_promenade_80180EBC;

extern SpriteBatch  D_acropolis_promenade_80183A20[2];
extern SpriteBatch  D_acropolis_promenade_80183FF8[11];
extern SpriteBatch  D_acropolis_promenade_801841A4[5];
extern SpriteBatch  D_acropolis_promenade_8018462C[9];
extern SpriteBatch  D_acropolis_promenade_80184CF0[9];
extern SpriteBatch  D_acropolis_promenade_80185224[9];
extern SpriteBatch  D_acropolis_promenade_801854EC[8];
extern SpriteBatch  D_acropolis_promenade_801858C4[9];
extern SpriteBatch  D_acropolis_promenade_80185F24[10];
extern SpriteBatch  D_acropolis_promenade_80185F74[2];
extern SpriteBatch  D_acropolis_promenade_80185F84[2];
extern SpriteBatch  D_acropolis_promenade_80185F94[2];
extern SpriteSource D_acropolis_promenade_80183A30[74];
extern SpriteSource D_acropolis_promenade_80184050[17];
extern SpriteSource D_acropolis_promenade_801841CC[56];
extern SpriteSource D_acropolis_promenade_80184674[83];
extern SpriteSource D_acropolis_promenade_80184D38[63];
extern SpriteSource D_acropolis_promenade_8018526C[32];
extern SpriteSource D_acropolis_promenade_8018552C[46];
extern SpriteSource D_acropolis_promenade_8018590C[78];
s32                 func_acropolis_promenade_8017D70C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                 func_acropolis_promenade_8017D8D8(Task*, s32, TaskMessageArg, TaskMessageArg);
s32                 func_acropolis_promenade_8017D8E0(Task*, s32, s32, TaskMessageArg);
s32                 func_acropolis_promenade_8017D938(Task*, s32, s32, TaskMessageArg);
void                func_acropolis_promenade_8017D930(void);
void                func_acropolis_promenade_8017D988(Task*);

TmdBone D_acropolis_promenade_8017FE04[1] = {
#include "assets/acropolis_promenade_model_03890_skeleton.inc"
};

u32 D_acropolis_promenade_8017FE28[1] = {
#include "assets/acropolis_promenade_model_03890_partVerts.inc"
};

SVECTOR D_acropolis_promenade_8017FE2C[171] = {
#include "assets/acropolis_promenade_model_03890_verts.inc"
};

u32 D_acropolis_promenade_80180384[691] = {
#include "assets/acropolis_promenade_model_03890_stream.inc"
};

TmdSource D_acropolis_promenade_80180E50 = {
    0,
    5480,
    0,
    1,
    D_acropolis_promenade_8017FE28,
    D_acropolis_promenade_8017FE2C,
    &D_acropolis_promenade_8017FE2C[171],
    D_acropolis_promenade_8017FE04,
    D_acropolis_promenade_80180384,
};

AcropolisPromenadeMsgEntry D_acropolis_promenade_80180E74[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, { .call0 = func_acropolis_promenade_8017D70C } },
    { 5104, { .call0 = func_acropolis_promenade_8017D8E0 } },
    { 5103, { .call3 = func_acropolis_promenade_8017D930 } },
    { 5105, { .call0 = func_acropolis_promenade_8017D8D8 } },
    { 5106, { .call0 = func_acropolis_promenade_8017D938 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_acropolis_promenade_80180EA4[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_acropolis_promenade_8017D988, { .model = &D_acropolis_promenade_80180E50 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_acropolis_promenade_80180EBC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_promenade_80180ED0[2] = {
    { { -1454, 171, -1843, 0 }, { 0, 2048, 0, 0 } },
    { { -999, 171, -2799, 0 }, { 0, 1324, 0, 0 } },
};

EvsCommand D_acropolis_promenade_80180F00[15] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_promenade_80180EBC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x510B000C }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x510B000F }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_acropolis_promenade_80181068[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s32 D_acropolis_promenade_80181140 = 0;

s32 D_acropolis_promenade_80181144 = 0;

TaskDesc D_acropolis_promenade_80181148[5] = {
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_promenade_8017DB9C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_promenade_8017DF74, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_promenade_8017DFD4, { .value = 0 } },
};

SVECTOR D_acropolis_promenade_80181184[300] = {
    { 1534, 5, 9250, 0 },
    { 1560, 5, 9250, 0 },
    { 1585, 5, 9250, 0 },
    { 1611, 5, 9250, 0 },
    { 1636, 5, 9250, 0 },
    { 1662, 5, 9250, 0 },
    { 1687, 5, 9250, 0 },
    { 1712, 5, 9250, 0 },
    { 1738, 5, 9250, 0 },
    { 1763, 5, 9250, 0 },
    { 1789, 5, 9250, 0 },
    { 1814, 5, 9250, 0 },
    { 1839, 2, 9250, 0 },
    { 1860, -11, 9250, 0 },
    { 1881, -26, 9250, 0 },
    { 1902, -40, 9250, 0 },
    { 1923, -54, 9250, 0 },
    { 1945, -68, 9250, 0 },
    { 1966, -82, 9250, 0 },
    { 1987, -96, 9250, 0 },
    { 2008, -110, 9250, 0 },
    { 2029, -124, 9250, 0 },
    { 2050, -138, 9250, 0 },
    { 2071, -152, 9250, 0 },
    { 2093, -167, 9250, 0 },
    { 2114, -181, 9250, 0 },
    { 2135, -195, 9250, 0 },
    { 2156, -209, 9250, 0 },
    { 2177, -223, 9250, 0 },
    { 2198, -237, 9250, 0 },
    { 2220, -251, 9250, 0 },
    { 2241, -265, 9250, 0 },
    { 2262, -279, 9250, 0 },
    { 2283, -294, 9250, 0 },
    { 2304, -308, 9250, 0 },
    { 2325, -322, 9250, 0 },
    { 2347, -336, 9250, 0 },
    { 2368, -350, 9250, 0 },
    { 2389, -364, 9250, 0 },
    { 2410, -378, 9250, 0 },
    { 2431, -392, 9250, 0 },
    { 2452, -406, 9250, 0 },
    { 2473, -420, 9250, 0 },
    { 2495, -435, 9250, 0 },
    { 2516, -449, 9250, 0 },
    { 2537, -463, 9250, 0 },
    { 2558, -477, 9250, 0 },
    { 2579, -491, 9250, 0 },
    { 2600, -505, 9250, 0 },
    { 2622, -519, 9250, 0 },
    { 2643, -533, 9250, 0 },
    { 2664, -547, 9250, 0 },
    { 2685, -562, 9250, 0 },
    { 2706, -576, 9250, 0 },
    { 2727, -590, 9250, 0 },
    { 2748, -604, 9250, 0 },
    { 2770, -618, 9250, 0 },
    { 2791, -632, 9250, 0 },
    { 2812, -646, 9250, 0 },
    { 2833, -660, 9250, 0 },
    { 2854, -674, 9250, 0 },
    { 2875, -688, 9250, 0 },
    { 2897, -703, 9250, 0 },
    { 2918, -717, 9250, 0 },
    { 2939, -731, 9250, 0 },
    { 2960, -745, 9250, 0 },
    { 2981, -759, 9250, 0 },
    { 3002, -773, 9250, 0 },
    { 3024, -787, 9250, 0 },
    { 3045, -801, 9250, 0 },
    { 3066, -815, 9250, 0 },
    { 3087, -829, 9250, 0 },
    { 3108, -844, 9250, 0 },
    { 3129, -858, 9250, 0 },
    { 3150, -872, 9250, 0 },
    { 3172, -886, 9250, 0 },
    { 3193, -900, 9250, 0 },
    { 3214, -914, 9250, 0 },
    { 3235, -928, 9250, 0 },
    { 3256, -942, 9250, 0 },
    { 3277, -956, 9250, 0 },
    { 3299, -971, 9250, 0 },
    { 3320, -985, 9250, 0 },
    { 3341, -999, 9250, 0 },
    { 3362, -1013, 9250, 0 },
    { 3383, -1027, 9250, 0 },
    { 3404, -1041, 9250, 0 },
    { 3425, -1055, 9250, 0 },
    { 3447, -1069, 9250, 0 },
    { 3468, -1083, 9250, 0 },
    { 3489, -1097, 9250, 0 },
    { 3510, -1112, 9250, 0 },
    { 3531, -1126, 9250, 0 },
    { 3552, -1140, 9250, 0 },
    { 3574, -1154, 9250, 0 },
    { 3595, -1168, 9250, 0 },
    { 3616, -1182, 9250, 0 },
    { 3637, -1196, 9250, 0 },
    { 3658, -1210, 9250, 0 },
    { 3679, -1224, 9250, 0 },
    { 3700, -1238, 9250, 0 },
    { 3722, -1253, 9250, 0 },
    { 3743, -1267, 9250, 0 },
    { 3764, -1281, 9250, 0 },
    { 3785, -1295, 9250, 0 },
    { 3806, -1309, 9250, 0 },
    { 3827, -1323, 9250, 0 },
    { 3849, -1337, 9250, 0 },
    { 3870, -1351, 9250, 0 },
    { 3891, -1365, 9250, 0 },
    { 3912, -1380, 9250, 0 },
    { 3933, -1394, 9250, 0 },
    { 3954, -1408, 9250, 0 },
    { 3976, -1422, 9250, 0 },
    { 3997, -1436, 9250, 0 },
    { 4018, -1450, 9250, 0 },
    { 4039, -1464, 9250, 0 },
    { 4060, -1478, 9250, 0 },
    { 4081, -1492, 9250, 0 },
    { 4102, -1506, 9250, 0 },
    { 4124, -1521, 9250, 0 },
    { 4145, -1535, 9250, 0 },
    { 4166, -1549, 9250, 0 },
    { 4187, -1563, 9250, 0 },
    { 4208, -1577, 9250, 0 },
    { 4229, -1591, 9250, 0 },
    { 4251, -1605, 9250, 0 },
    { 4272, -1619, 9250, 0 },
    { 4293, -1633, 9250, 0 },
    { 4314, -1648, 9250, 0 },
    { 4335, -1662, 9250, 0 },
    { 4356, -1676, 9250, 0 },
    { 4377, -1690, 9250, 0 },
    { 4399, -1704, 9250, 0 },
    { 4420, -1718, 9250, 0 },
    { 4441, -1732, 9250, 0 },
    { 4462, -1746, 9250, 0 },
    { 4483, -1760, 9250, 0 },
    { 4504, -1774, 9250, 0 },
    { 4526, -1789, 9250, 0 },
    { 4547, -1803, 9250, 0 },
    { 4568, -1817, 9250, 0 },
    { 4589, -1831, 9250, 0 },
    { 4610, -1845, 9250, 0 },
    { 4631, -1859, 9250, 0 },
    { 4653, -1873, 9250, 0 },
    { 4674, -1887, 9250, 0 },
    { 4695, -1901, 9250, 0 },
    { 4716, -1915, 9250, 0 },
    { 4737, -1930, 9250, 0 },
    { 4758, -1944, 9250, 0 },
    { 4779, -1958, 9250, 0 },
    { 4801, -1972, 9250, 0 },
    { 4822, -1986, 9250, 0 },
    { 4843, -2000, 9250, 0 },
    { 4864, -2014, 9250, 0 },
    { 4885, -2028, 9250, 0 },
    { 4906, -2042, 9250, 0 },
    { 4928, -2057, 9250, 0 },
    { 4949, -2071, 9250, 0 },
    { 4970, -2085, 9250, 0 },
    { 4991, -2099, 9250, 0 },
    { 5012, -2113, 9250, 0 },
    { 5033, -2127, 9250, 0 },
    { 5054, -2141, 9250, 0 },
    { 5076, -2155, 9250, 0 },
    { 5097, -2169, 9250, 0 },
    { 5118, -2183, 9250, 0 },
    { 5139, -2198, 9250, 0 },
    { 5160, -2212, 9250, 0 },
    { 5181, -2226, 9250, 0 },
    { 5203, -2240, 9250, 0 },
    { 5224, -2254, 9250, 0 },
    { 5245, -2268, 9250, 0 },
    { 5266, -2282, 9250, 0 },
    { 5287, -2296, 9250, 0 },
    { 5308, -2310, 9250, 0 },
    { 5330, -2325, 9250, 0 },
    { 5351, -2339, 9250, 0 },
    { 5372, -2353, 9250, 0 },
    { 5393, -2367, 9250, 0 },
    { 5414, -2381, 9250, 0 },
    { 5435, -2395, 9250, 0 },
    { 5456, -2409, 9250, 0 },
    { 5478, -2423, 9250, 0 },
    { 5499, -2437, 9250, 0 },
    { 5520, -2451, 9250, 0 },
    { 5541, -2466, 9250, 0 },
    { 5562, -2480, 9250, 0 },
    { 5583, -2494, 9250, 0 },
    { 5605, -2508, 9250, 0 },
    { 5626, -2522, 9250, 0 },
    { 5647, -2536, 9250, 0 },
    { 5668, -2550, 9250, 0 },
    { 5689, -2564, 9250, 0 },
    { 5710, -2578, 9250, 0 },
    { 5731, -2592, 9250, 0 },
    { 5753, -2607, 9250, 0 },
    { 5774, -2621, 9250, 0 },
    { 5795, -2635, 9250, 0 },
    { 5816, -2649, 9250, 0 },
    { 5837, -2663, 9250, 0 },
    { 5858, -2677, 9250, 0 },
    { 5880, -2691, 9250, 0 },
    { 5901, -2705, 9250, 0 },
    { 5922, -2719, 9250, 0 },
    { 5943, -2734, 9250, 0 },
    { 5964, -2748, 9250, 0 },
    { 5985, -2762, 9250, 0 },
    { 6007, -2776, 9250, 0 },
    { 6028, -2790, 9250, 0 },
    { 6049, -2804, 9250, 0 },
    { 6070, -2818, 9250, 0 },
    { 6091, -2832, 9250, 0 },
    { 6112, -2846, 9250, 0 },
    { 6133, -2860, 9250, 0 },
    { 6155, -2875, 9250, 0 },
    { 6176, -2889, 9250, 0 },
    { 6197, -2903, 9250, 0 },
    { 6218, -2917, 9250, 0 },
    { 6239, -2931, 9250, 0 },
    { 6260, -2945, 9250, 0 },
    { 6282, -2959, 9250, 0 },
    { 6303, -2973, 9250, 0 },
    { 6324, -2987, 9250, 0 },
    { 6347, -2995, 9250, 0 },
    { 6373, -2995, 9250, 0 },
    { 6398, -2995, 9250, 0 },
    { 6423, -2995, 9250, 0 },
    { 6449, -2995, 9250, 0 },
    { 6474, -2995, 9250, 0 },
    { 6500, -2995, 9250, 0 },
    { 6525, -2995, 9250, 0 },
    { 6551, -2995, 9250, 0 },
    { 6576, -2995, 9250, 0 },
    { 6601, -2995, 9250, 0 },
    { 6627, -2995, 9250, 0 },
    { 6652, -2995, 9250, 0 },
    { 6678, -2995, 9250, 0 },
    { 6703, -2995, 9250, 0 },
    { 6729, -2995, 9250, 0 },
    { 6754, -2995, 9250, 0 },
    { 6779, -2995, 9250, 0 },
    { 6805, -2995, 9250, 0 },
    { 6830, -2995, 9250, 0 },
    { 6856, -2995, 9250, 0 },
    { 6881, -2995, 9250, 0 },
    { 6907, -2995, 9250, 0 },
    { 6932, -2995, 9250, 0 },
    { 6957, -2995, 9250, 0 },
    { 6983, -2995, 9250, 0 },
    { 7008, -2995, 9250, 0 },
    { 7034, -2995, 9250, 0 },
    { 7059, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
    { 7085, -2995, 9250, 0 },
};

ApmGlowCorner D_acropolis_promenade_80181AE4[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

// Indexed views below share one contiguous table.
SVECTOR D_acropolis_promenade_80181AF4[3] = {
    { -4700, -1000, -620, 0 },
    { -3920, -1494, -3450, 0 },
    { -6900, -1494, -3564, 0 },
};

SVECTOR D_acropolis_promenade_80181B0C[1] = {
    { -6900, 300, -2543, 0 },
};

SVECTOR D_acropolis_promenade_80181B14[12] = {
    { 810, -3000, -3720, 0 },
    { 650, -3000, -4000, 0 },
    { 810, -3000, -4270, 0 },
    { -2470, -2050, -740, 0 },
    { -2520, -2050, -3120, 0 },
    { -16000, -1700, -1330, 0 },
    { -16000, -1700, -2700, 0 },
    { 1680, -1700, 10080, 0 },
    { 1680, -1700, 8430, 0 },
    { 1640, -360, 7020, 0 },
    { -2150, -360, 8460, 0 },
    { -3400, -370, 2300, 0 },
};

u16 D_acropolis_promenade_80181B74 = 32;

u16 D_acropolis_promenade_80181B76 = 32;

u16 D_acropolis_promenade_80181B78[12] = {
    442,
    440,
    440,
    401,
    401,
    18,
    18,
    478,
    510,
    64,
    512,
    32,
};

GpRoomObjRec D_acropolis_promenade_80181B90[2] = {
    { D_acropolis_promenade_801823DC, D_acropolis_promenade_80182BF4, D_acropolis_promenade_80182DBC, NULL },
    { D_acropolis_promenade_80182BD0, D_acropolis_promenade_80182BF4, D_acropolis_promenade_80182DBC, NULL },
};

u8 D_acropolis_promenade_80181BB0[16] = {
    1,
    2,
    3,
    8,
    9,
    6,
    7,
    4,
    5,
    10,
    11,
    12,
    13,
    0,
    0,
    0,
};

u8* D_acropolis_promenade_80181BC0[2] = {
    D_8010CAF8,
    D_acropolis_promenade_80181BB0,
};

GpViewCountRec D_acropolis_promenade_80181BC8[2] = {
    { { .bytes = { 13, 0 } } },
    { { .bytes = { 13, 0 } } },
};

GpRoomCoordRec D_acropolis_promenade_80181BCC[2] = {
    { D_acropolis_promenade_80183A08, NULL },
    { D_acropolis_promenade_80183A08, NULL },
};

GpWarpRec D_acropolis_promenade_80181BDC[5] = {
    { { .words = { 3072, 642, 42, 9120 } }, { 0, 0, 0, 0 }, { .words = { 3072, 642, 42, 9120 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 3072, 373, 42, -4230 } }, { 0, 0, 0, 0 }, { .words = { 3072, 373, 42, -4230 } }, { 0, 0, 0, 0 }, 0x510B0006, 0x510B0005, 0, 4, 0, 493 },
    { { .words = { 1024, -0x3D4E, 0, -2098 } }, { 0, 0, 0, 0 }, { .words = { 1024, -0x3D4E, 0, -2098 } }, { 0, 0, 0, 0 }, 0x510B0008, 0x510B0007, 0, 8, 0, 0 },
    { { .words = { 3072, 642, 42, 9120 } }, { 0, 0, 0, 0 }, { .words = { 3072, 642, 42, 9120 } }, { 0, 0, 0, 0 }, 0, 0, 0, 7, 0, 0 },
    { { .words = { 1024, -2197, 44, -1977 } }, { 0, 0, 0, 0 }, { .words = { 1024, -2197, 44, -1977 } }, { 0, 0, 0, 0 }, 0x510B000E, 0x510B000D, 0, 4, 0, 491 },
};

SVECTOR D_acropolis_promenade_80181CF4[30] = {
#include "assets/acropolis_promenade_collision_04E1C_normals.inc"
};

SVECTOR D_acropolis_promenade_80181DE4[93] = {
#include "assets/acropolis_promenade_collision_04E1C_verts.inc"
};

WorldCollisionGridFace D_acropolis_promenade_801820CC[38] = {
#include "assets/acropolis_promenade_collision_04E1C_faces.inc"
};

s16 D_acropolis_promenade_80182294[144] = {
#include "assets/acropolis_promenade_collision_04E1C_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_promenade_80182294[i])
s16* D_acropolis_promenade_801823B4[10] = {
#include "assets/acropolis_promenade_collision_04E1C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_promenade_801823DC[1] = {
    { NULL, D_acropolis_promenade_80181CF4, D_acropolis_promenade_80181DE4, D_acropolis_promenade_801820CC, D_acropolis_promenade_801823B4, 2780, 8590, 2, 5, 4000, 38 },
};

SVECTOR D_acropolis_promenade_80182400[30] = {
#include "assets/acropolis_promenade_collision_05610_normals.inc"
};

SVECTOR D_acropolis_promenade_801824F0[103] = {
#include "assets/acropolis_promenade_collision_05610_verts.inc"
};

WorldCollisionGridFace D_acropolis_promenade_80182828[46] = {
#include "assets/acropolis_promenade_collision_05610_faces.inc"
};

s16 D_acropolis_promenade_80182A50[172] = {
#include "assets/acropolis_promenade_collision_05610_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_promenade_80182A50[i])
s16* D_acropolis_promenade_80182BA8[10] = {
#include "assets/acropolis_promenade_collision_05610_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_promenade_80182BD0[1] = {
    { NULL, D_acropolis_promenade_80182400, D_acropolis_promenade_801824F0, D_acropolis_promenade_80182828, D_acropolis_promenade_80182BA8, 2780, 8590, 2, 5, 4000, 46 },
};

GpObj4C D_acropolis_promenade_80182BF4[6] = {
    { NULL, NULL, NULL, { -802, -2544, 510, 0 }, { { -1820, -4175, 362, 0 }, { 1820, -4209, -362, 0 }, { -1820, 4209, 362, 0 }, { 1820, 4175, -362, 0 } }, { -800, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -804, -2432, 91, 0 }, { { 1820, -4175, -361, 0 }, { -1820, -4209, 362, 0 }, { 1820, 4209, -361, 0 }, { -1820, 4175, 362, 0 } }, { 797, 0, 4017, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -288, -2272, 4160, 0 }, { { -1856, -4175, 0, 0 }, { 1856, -4209, 0, 0 }, { -1856, 4209, 0, 0 }, { 1856, 4175, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -256, -2432, 3681, 0 }, { { 1856, -4175, 0, 0 }, { -1856, -4209, 0, 0 }, { 1856, 4209, 0, 0 }, { -1856, 4175, 0, 0 } }, { 0, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -997, -2624, -5124, 0 }, { { 1843, -4175, -185, 0 }, { -1850, -4209, 179, 0 }, { 1843, 4209, -185, 0 }, { -1850, 4175, 179, 0 } }, { 401, 0, 4076, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -999, -2560, -4839, 0 }, { { -1854, -4175, 90, 0 }, { 1851, -4209, -93, 0 }, { -1854, 4209, 90, 0 }, { 1851, 4175, -93, 0 } }, { -203, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 5, 4, 129, 0 },
};

GpObj4C D_acropolis_promenade_80182DBC[6] = {
    { NULL, NULL, NULL, { 989, -112, 9028, 0 }, { { -1024, 0, -1024, 0 }, { 1024, 0, -1024, 0 }, { -1024, 0, 1024, 0 }, { 1024, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1448, 0, 10, 67, 2, 0 },
    { NULL, NULL, NULL, { 752, -112, -3903, 0 }, { { -496, 0, -1215, 0 }, { 496, 0, -1215, 0 }, { -496, 0, 1216, 0 }, { 496, 0, 1216, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1311, 0, 12, 33, 2, 0 },
    { NULL, NULL, NULL, { -1408, -105, 7680, 0 }, { { -1024, 0, -1088, 0 }, { 1024, 0, -1088, 0 }, { -1024, 0, 1088, 0 }, { 1024, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, 201, 0 }, 1492, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { -2432, -96, -8512, 0 }, { { -1024, 0, -1024, 0 }, { 1568, 0, -1024, 0 }, { -1024, 0, 1472, 0 }, { 1568, 0, 1472, 0 } }, { 0, 4097, 0, 0 }, { 600, 0, 4051, 0 }, 2141, 6, 5, 0, 4, 0 },
    { NULL, NULL, NULL, { -2208, -128, -2048, 0 }, { { -576, 0, -1024, 0 }, { 576, 0, -1024, 0 }, { -576, 0, 1024, 0 }, { 576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4094, 0, -1, 0 }, 1173, 0, 14, 81, 2, 0 },
    { NULL, NULL, NULL, { -1152, -64, -2608, 0 }, { { -2336, 0, -272, 0 }, { 2336, 0, -752, 0 }, { -2336, 0, 752, 0 }, { 2336, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2442, 0x8005, 6, 0, 131, 0 },
};

GpAreaTmdRec D_acropolis_promenade_80182F84[2] = {
    { 11, 11, 2, 0, { 0, 0 }, D_80177400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_promenade_80182F9C[3] = {
    { 7, 7, 1, 0, { 0, 0 }, D_80150C80 },
    { 8, 7, 1, 0, { 0, 0 }, D_801513C8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_promenade_80182FC0[3] = {
    { 55, 55, 0, 0, { 0, 0 }, D_8013A8DC },
    { 8, 7, 1, 0, { 0, 0 }, D_801513C8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_promenade_80182FE4[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_promenade_80182FFC[3] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 8, 7, 1, 0, { 0, 0 }, D_801513C8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_acropolis_promenade_80183020[17] = {
    { NULL, NULL },
    { D_map_akropolis_8017B82C, D_acropolis_promenade_80182F84 },
    { D_map_akropolis_8017B85C, D_acropolis_promenade_80182F9C },
    { D_map_akropolis_8017B8DC, D_acropolis_promenade_80182FC0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B98C, D_acropolis_promenade_80182FE4 },
    { D_map_akropolis_8017B9AC, D_acropolis_promenade_80182FFC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordPointLight D_acropolis_promenade_801830A8[25] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1367, -507, 6799 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 2457 }, { 0, 0 } }, 500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -335, -2500, 3516 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 4096 }, { 0, 0 } }, 0, 0x2B5C },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1970, -507, 8207 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 2457 }, { 0, 0 } }, 500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1342, -2000, 0x2854 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 4096 }, { 0, 0 } }, 200, 9000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -80, -2500, -3995 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 4096 }, { 0, 0 } }, 500, 0x3714 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2333, -2000, -763 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3276, 2867 }, { 0, 0 } }, 200, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2333, -2000, -3192 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3276, 2867 }, { 0, 0 } }, 200, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6854, -1500, -3373 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2457, 2457 }, { 0, 0 } }, 500, 3955 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9907, -1500, -3373 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2457, 2457 }, { 0, 0 } }, 500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x324D, -1500, -3373 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2457, 2457 }, { 0, 0 } }, 500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x33A3, -1500, -719 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2457, 2457 }, { 0, 0 } }, 500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2808, -1500, -719 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2457, 2457 }, { 0, 0 } }, 500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7195, -1500, -719 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3604, 2457, 2457 }, { 0, 0 } }, 500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x38AB, 1600, -4070 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 7500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2D1E, 1600, -4070 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 7500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5464, 1600, -4070 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 5179 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8422, 1600, -4070 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 7500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8422, 1600, -22 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 7500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5464, 1600, -22 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 501, 3961 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2D1E, 1600, -22 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 7500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x38AB, 1600, -22 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3686, 4096 }, { 0, 0 } }, 500, 7500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3D7E, 2000, -3035 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 3276 }, { 0, 0 } }, 200, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3D7E, 2000, -994 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 3276 }, { 0, 0 } }, 200, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -80, -2500, -8518 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 4096 }, { 0, 0 } }, 500, 0x3714 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4630, -1500, -3219 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2457, 2457 }, { 0, 0 } }, 0, 2 },
};

GpRoomCoordSet D_acropolis_promenade_80183A08[1] = {
    { 0, NULL, 25, D_acropolis_promenade_801830A8, 0, NULL },
};

SpriteBatch D_acropolis_promenade_80183A20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80183A30[74] = {
    { 141, 0x3FC0, { .fields = { 72, 24 } }, -160, 96, 450, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -160, -32, 450, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 24, 450, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -120, 427, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -32, -112, 435, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -152, -120, 875, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -120, -104, 951, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, -80, 988, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -80, 1148, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -72, 1096, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -56, 1040, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -48, 994, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, -72, -48, 963, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -40, -56, 950, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -40, -120, 950, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -32, -104, 950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -72, 950, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -80, -24, 973, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -144, -24, 875, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 875, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -104, 8, 925, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, 8, 925, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 32, 925, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -64, 56, 925, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 875, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, 32, 875, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 48, 875, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -112, 56, 925, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, -40, 875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -120, 487, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -48, 950, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -160, -8, 462, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -112, 8, 462, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -128, 64, 462, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -160, 80, 462, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, -152, 96, 450, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -160, -112, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -144, -112, 450, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -136, -120, 450, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -88, -120, 437, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, -120, 625, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, -120, 750, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -32, -120, 875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -24, -96, 925, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -88, 925, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 80, 465, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -88, -32, 950, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -96, 0, 950, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 509, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 524, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -96, -120, 974, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -104, -24, 962, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 128 } }, 104, -40, 1500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -32, 1750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 80, -8, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -160, -120, 950, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 56 } }, -144, -120, 950, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 72 } }, -128, -120, 950, { .fields = { 8, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 88 } }, -104, -120, 950, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 80 } }, -40, -120, 950, { .fields = { 104, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 72 } }, -160, -32, 876, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 80 } }, -144, -24, 876, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 88 } }, -128, -8, 876, { .fields = { 8, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 72 } }, -104, 8, 926, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 8 } }, -8, -48, 925, { .fields = { 64, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 16, 32 } }, -160, -120, 500, { .fields = { 104, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 40, 24 } }, -144, -120, 500, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 40, 16 } }, -104, -120, 500, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 32, 40 } }, -64, -120, 750, { .fields = { 120, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 80 } }, -32, -120, 875, { .fields = { 80, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 104 } }, -160, -8, 463, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 24, 96 } }, -128, -8, 463, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 64 } }, -104, 8, 463, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_promenade_80183FF8[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 2, 0, 0, { 5, 0 } },
    { 5, 25, 0, 0, { 4, 0 } },
    { 30, 16, 0, 0, { 6, 0 } },
    { 46, 3, 0, 0, { 1, 0 } },
    { 49, 4, 0, 0, { 7, 0 } },
    { 53, 3, 0, 0, { 3, 0 } },
    { 56, 9, 0, 0, { 8, 0 } },
    { 65, 9, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80184050[17] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -48, 1525, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -40, 1568, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 120 } }, -160, -120, 1450, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 104 } }, -160, 0, 1450, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, -112, -120, 1450, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -112, 0, 1450, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, 0, 1525, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, 0, 1558, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -120, 1083, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 80, -120, 988, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 40, 930, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 40, 925, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 80, -96, 968, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 0, 925, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 24, 2499, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 24, 2498, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 24, 2449, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_801841A4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 6, 0, 0, { 2, 0 } },
    { 14, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_801841CC[56] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 64, 1067, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -160, -112, 587, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -152, -112, 659, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -144, -112, 731, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -136, -112, 800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -128, -112, 877, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -120, -112, 954, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -112, -112, 1112, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -56, 1112, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 24, 591, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 24, 664, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 707, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 24, 800, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 24, 882, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, 24, 959, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 1087, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 1250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, -112, -88, 1314, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -96, -72, 1826, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -88, -40, 2217, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -72, 1465, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -32, 2776, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -32, 2844, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -32, 2932, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -32, 2842, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 16, -56, 1500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 24, -72, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1400, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, 32, 720, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 136, 32, 715, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 32, 707, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 104, 609, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -120, 595, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 152, 8, 583, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, 24, 741, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 16, 2404, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -48, 2192, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -16, 2214, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 32, 1700, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 40, 1600, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 48, 1450, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -160, 56, 1300, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -160, 72, 1075, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 80, 975, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -152, 88, 862, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 96, 787, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 104, 750, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 112, 700, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, 64, 1175, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 16, 1250, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 32, 1250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 40, 2200, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -160, 8, 2200, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 16, 2200, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 16, 2200, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 16, 2200, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_8018462C[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 9, 0, 0, { 4, 0 } },
    { 25, 10, 0, 0, { 1, 0 } },
    { 35, 3, 0, 0, { 5, 0 } },
    { 38, 11, 0, 0, { 0, 0 } },
    { 49, 2, 0, 0, { 6, 0 } },
    { 51, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80184674[83] = {
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, -80, 3667, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -80, 3667, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -80, 3667, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -152, 80, 625, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -112, 88, 412, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -80, 88, 609, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -56, 80, 618, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, 72, 637, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, 8, 56, 631, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 625, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 56 } }, 48, 64, 625, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 96, 48, 625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 128, 40, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 936, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 56, 940, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 830, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 64, 854, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 64, 911, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 64, 972, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 64, 882, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 795, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 821, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 72, 924, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 72, 849, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 72, 851, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 72, 865, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 72, 625, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 96, 625, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 112, 625, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, 112, 625, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 625, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 80, 685, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 88, 725, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -56, 88, 739, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 850, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -24, 1500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 144 } }, 40, -112, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 2200, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -8, 1975, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 0, 1752, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -32, 1975, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -144, -40, 1975, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -96, -48, 1760, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -40, 3311, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -80, -32, 2853, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, 80, 974, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 88, 925, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 96, 900, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -152, 104, 875, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 32, 1272, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 24, 1355, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 1467, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 8, 1622, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 40, 1170, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1094, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 56, 1036, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 64, 1006, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 975, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 2750, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, -64, 1100, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -160, 8, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -152, 40, 1100, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -120, 301, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -72, -64, 3148, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -80, -96, 2375, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -88, -104, 1906, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -96, -104, 1375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -104, -104, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -144, -104, 855, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -152, -104, 755, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -160, -104, 855, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -112, -104, 1031, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, -80, 1025, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -128, 0, 811, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -120, -8, 850, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -128, -80, 850, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -136, 8, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -88, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -112, -8, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -152, 48, 1025, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -160, 56, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 64, 1000, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 875, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_80184CF0[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 22, 0, 0, { 4, 0 } },
    { 35, 2, 0, 0, { 1, 0 } },
    { 37, 22, 0, 0, { 5, 0 } },
    { 59, 3, 0, 0, { 0, 0 } },
    { 62, 17, 0, 0, { 6, 0 } },
    { 79, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_80184D38[63] = {
    { 143, 0x3FC0, { .fields = { 24, 176 } }, -160, -120, 1475, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -136, -56, 1500, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 96 } }, -120, -48, 1525, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, -96, -40, 1525, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 8, -64, 1525, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, 40, -72, 1525, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -48, 1525, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 72, -24, 1525, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 88, -48, 1525, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 96, -24, 1525, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 112, -48, 1525, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 120, -32, 1525, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 112 } }, -40, -64, 1525, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -72, -8, 750, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -48, 8, 750, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -24, 40, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 24, 8, 750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 64 } }, 56, -48, 1150, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -24, 1150, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 112, -48, 1150, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, 64, 16, 1125, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, -104, -8, 750, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 24, 788, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 32 } }, 80, -120, 461, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 32 } }, 64, -88, 457, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, 72, -56, 465, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 32 } }, 88, -32, 472, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 104, 0, 452, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 96 } }, 88, 24, 416, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 24, 825, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -72, 40, 825, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -80, 64, 825, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, 72, 825, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 72, 825, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -32, 80, 825, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 32, 825, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 24, 80, 825, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 96, 825, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 96, 920, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, 40, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, 40, 800, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 88, 889, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -48, 96, 857, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 825, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, 40, 798, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, 96, 818, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 32, 40, 868, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 112, 800, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 56, 700, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -80, 112, 869, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, 56, 775, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 80, 739, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 112, 450, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 88, 609, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -152, 88, 638, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -144, 88, 639, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 80, 677, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 80, 707, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 72, 780, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 829, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, 96, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 80, 580, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, 80, 671, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_80185224[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 3, 0 } },
    { 13, 9, 0, 0, { 4, 0 } },
    { 22, 7, 0, 0, { 1, 0 } },
    { 29, 9, 0, 0, { 5, 0 } },
    { 38, 9, 0, 0, { 0, 0 } },
    { 47, 4, 0, 0, { 6, 0 } },
    { 51, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_8018526C[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 550, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, -40, -120, 550, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -112, 550, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -72, -96, 550, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 112 } }, -56, -120, 500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 104 } }, -8, -120, 500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -72, -80, 500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -56, 500, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, 72, -104, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, 24, -72, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 48 } }, 64, -56, 750, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 120 } }, -160, -120, 475, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, 40, 875, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 120 } }, -160, 0, 875, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, 96, 137, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -88, 8, 750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 48, 8, 1096, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, 8, 1050, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, 16, 1012, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 72, 24, 975, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 80, 32, 950, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 88, 32, 935, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, 32, 925, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 112, 24, 913, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, 24, 925, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 144, 24, 950, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 72 } }, -72, -80, 487, { .fields = { 16, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 80, 64 } }, -8, -80, 487, { .fields = { 0, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 56 } }, 72, -80, 487, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 88, 40 } }, -80, -120, 487, { .fields = { 56, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 80, 40 } }, 8, -120, 487, { .fields = { 80, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_promenade_801854EC[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 5, 0, 0, { 0, 0 } },
    { 9, 2, 0, 0, { 5, 0 } },
    { 11, 5, 0, 0, { 1, 0 } },
    { 16, 11, 0, 0, { 4, 0 } },
    { 27, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_8018552C[46] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -48, 1125, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -96, 250, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -152, -104, 625, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -144, -96, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -136, -96, 775, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -128, -88, 850, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -120, -96, 875, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -112, -96, 1057, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 168 } }, -104, -96, 1087, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 40, 1087, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -48, 1125, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -96, 1125, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -56, 1600, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -112, -64, 1600, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 112 } }, -96, -64, 1600, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, -64, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -32, 2500, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -56, -24, 3050, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, -24, 3075, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -24, 3100, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 152, -120, 700, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 144, -32, 700, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 144, 32, 700, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, 120, 32, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 24, -56, 2000, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 16, 2025, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 40, 1775, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 40, 1775, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 48, 1512, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 48, 1512, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 56, 1275, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 56, 1275, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 64, 1175, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 104, 750, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 112, 700, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, 64, 994, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -128, 72, 1075, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, 80, 975, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -144, 88, 862, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -152, 96, 787, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -160, 16, 1087, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 32, 1087, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 32, 1087, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -136, -56, 1600, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -64, 1750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 112 } }, -96, -64, 1750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_801858C4[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 8, 0, 0, { 4, 0 } },
    { 20, 4, 0, 0, { 1, 0 } },
    { 24, 2, 0, 0, { 5, 0 } },
    { 26, 14, 0, 0, { 0, 0 } },
    { 40, 3, 0, 0, { 6, 0 } },
    { 43, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_promenade_8018590C[78] = {
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -152, 80, 600, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -112, 88, 600, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -56, 80, 600, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, 72, 600, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 8, 56, 600, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 600, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 56 } }, 48, 64, 600, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 80 } }, 96, 40, 600, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 936, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 56, 940, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 830, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 64, 854, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 64, 911, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 64, 972, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 64, 882, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 795, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 821, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 72, 924, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -32, 72, 849, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 72, 851, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 72, 865, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -112, 72, 625, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, 80, 612, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -72, 80, 612, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -96, 104, 612, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, 104, 612, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 88, 596, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 88, 792, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 80, 849, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, 80, 865, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 661, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 40, -112, 1350, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 32, -24, 1375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, -48, 3275, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -144, -40, 3250, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -32, 3075, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -24, 2600, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -16, 2275, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, -8, 1975, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -160, 0, 1800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 8, 1752, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 16, 1622, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -152, 88, 974, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -152, 96, 925, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -152, 104, 900, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 112, 875, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 24, 1467, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 1355, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 40, 1272, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1170, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 56, 1094, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 64, 1036, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 1006, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -144, 80, 975, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -160, 8, 1125, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 72 } }, -160, -64, 1125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -160, 48, 900, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -160, 72, 850, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, 88, 800, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -80, 3825, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -80, 3825, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -80, 3800, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, -96, 3000, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -96, 2500, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -112, -96, 2500, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -96, 2500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -72, 1665, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -24, 1865, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -88, -104, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -96, -104, 1625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -104, -104, 1550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, -112, -104, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -120, -80, 1300, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 200 } }, -128, -80, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 208 } }, -136, -88, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -144, -88, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -152, -88, 900, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 192 } }, -160, -88, 800, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_promenade_80185F24[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 6, 0 } },
    { 8, 23, 0, 0, { 1, 0 } },
    { 31, 2, 0, 0, { 4, 0 } },
    { 33, 21, 0, 0, { 0, 0 } },
    { 54, 2, 0, 0, { 5, 0 } },
    { 56, 3, 0, 0, { 3, 0 } },
    { 59, 7, 0, 0, { 7, 0 } },
    { 66, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185F74[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185F84[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185F94[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_promenade_80185FA4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_acropolis_promenade_80185FB4[13] = {
    { { .empty = D_acropolis_promenade_80183A20 }, D_acropolis_promenade_80183A20, NULL },
    { { .elements = D_acropolis_promenade_80183A30 }, D_acropolis_promenade_80183FF8, NULL },
    { { .elements = D_acropolis_promenade_80184050 }, D_acropolis_promenade_801841A4, NULL },
    { { .elements = D_acropolis_promenade_801841CC }, D_acropolis_promenade_8018462C, NULL },
    { { .elements = D_acropolis_promenade_80184674 }, D_acropolis_promenade_80184CF0, NULL },
    { { .elements = D_acropolis_promenade_80184D38 }, D_acropolis_promenade_80185224, NULL },
    { { .elements = D_acropolis_promenade_8018526C }, D_acropolis_promenade_801854EC, NULL },
    { { .elements = D_acropolis_promenade_8018552C }, D_acropolis_promenade_801858C4, NULL },
    { { .elements = D_acropolis_promenade_8018590C }, D_acropolis_promenade_80185F24, NULL },
    { { .empty = D_acropolis_promenade_80185F74 }, D_acropolis_promenade_80185F74, NULL },
    { { .empty = D_acropolis_promenade_80185F84 }, D_acropolis_promenade_80185F84, NULL },
    { { .empty = D_acropolis_promenade_80185F94 }, D_acropolis_promenade_80185F94, NULL },
    { { .empty = D_acropolis_promenade_80183A20 }, D_acropolis_promenade_80183A20, NULL },
};

GpViewRec D_acropolis_promenade_80186050[13] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 607, 0x7530, -6098 } }, 499 },
    { { { { -3956, 0, -1059 }, { 241, 3987, -903 }, { 1031, -935, -3852 } }, { 372, 488, -0x2DD6 } }, 230 },
    { { { { 3958, 0, -1053 }, { 187, 4030, 703 }, { 1036, -728, 3895 } }, { 1617, 462, 2297 } }, 230 },
    { { { { 3947, 0, -1092 }, { -16, 4095, -57 }, { 1092, 60, 3947 } }, { 1820, 1322, 7620 } }, 230 },
    { { { { 3977, 0, -977 }, { -306, 3889, -1247 }, { 928, 1284, 3776 } }, { 1842, 2326, 0x29C5 } }, 230 },
    { { { { 1010, 0, -3969 }, { 299, 4084, 76 }, { 3957, -309, 1007 } }, { 9221, 696, 3327 } }, 230 },
    { { { { -1131, 0, -3936 }, { -1680, 3703, 483 }, { 3559, 1748, -1023 } }, { 1524, 2795, -0x2739 } }, 230 },
    { { { { 4061, 0, -530 }, { 2, 4095, 21 }, { 530, -21, 4061 } }, { 1461, 1274, 7835 } }, 230 },
    { { { { 3977, 0, -977 }, { -306, 3889, -1247 }, { 928, 1284, 3776 } }, { 1842, 2326, 0x29C5 } }, 230 },
    { { { { -1592, 0, 3773 }, { 1874, 3555, 790 }, { -3275, 2034, -1381 } }, { 112, 2168, -7895 } }, 230 },
    { { { { -1151, 0, 3930 }, { 1811, 3634, 530 }, { -3488, 1887, -1022 } }, { 1036, 1013, 7696 } }, 230 },
    { { { { 4078, 0, -382 }, { -286, 2709, -3058 }, { 252, 3072, 2697 } }, { 1927, 1344, -1461 } }, 230 },
    { { { { 3076, 0, -2704 }, { -116, 4092, -133 }, { 2701, 177, 3073 } }, { 1791, 1265, 5187 } }, 257 },
};

PadScriptCmd D_acropolis_promenade_80186224[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 22), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_promenade_8018623C[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186244 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186250 = {
    0x10000031,
    0x10000033,
    0x10000031,
};

WorldCollisionFootstepSounds D_acropolis_promenade_8018625C = {
    0x10000025,
    0x10000027,
    0x10000025,
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186268 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionFootstepSounds D_acropolis_promenade_80186274 = {
    0x10000025,
    0x10000027,
    0x10000025,
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186280[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186288[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_80186244 },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186290[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_80186250 },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_80186298[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_8018625C },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_801862A0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_promenade_80186268 },
};

WorldCollisionSurfaceProperties D_acropolis_promenade_801862A8[1] = {
    { 1, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_SUPPRESS_PUSHBACK, &D_acropolis_promenade_80186274 },
};

WorldCollisionSurfaceProperties* D_acropolis_promenade_801862B0[8] = {
    D_acropolis_promenade_80186280,
    D_acropolis_promenade_80186288,
    D_acropolis_promenade_80186290,
    D_acropolis_promenade_80186298,
    D_acropolis_promenade_801862A0,
    D_acropolis_promenade_80186280,
    D_acropolis_promenade_80186280,
    D_acropolis_promenade_801862A8,
};

RoomEventMsg D_acropolis_promenade_801862D0;

Task* D_acropolis_promenade_801862D8;

static void func_acropolis_promenade_8017D5E4(Task* task);

/// Per-frame state of the room task. The first frame the session's warp is 4
/// it spawns the streamed-scene task (entry 2 of the task table), once. While
/// the location's place is 1 it keeps `flowFlags` at 0xA and runs a latch on
/// `gSceneCombatState.signals.bytes.battlePhase`: when that flag drops after having been 1, a sound
/// event is queued, and once the session's `battleResetPending` is then non-zero,
/// `func_800E8634` is called with the room's two data blocks.
static void func_acropolis_promenade_8017D5E4(Task* task)
{
    u8 temp;
    u8 f0;

    if (D_acropolis_promenade_80181140 == 0) {
        if (gGameSession->location.loc.warp == 4) {
            D_acropolis_promenade_80181140 = 1;
            Task_SpawnFromTable(D_acropolis_promenade_80181148, 2, 0, 0);
        }
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent == 6) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 5;
    }
    temp = gGameSession->location.loc.variant;
    if (temp == 1) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY);
        f0                      = gSceneCombatState.signals.bytes.battlePhase;
        if (f0 == temp) {
            D_acropolis_promenade_80181144 = f0;
        }
        if ((D_acropolis_promenade_80181144 == temp) && (f0 != D_acropolis_promenade_80181144)) {
            D_acropolis_promenade_80181144 = 2;
            SndEvt_EnqueueType2(0, 0x3C);
        }
        if ((D_acropolis_promenade_80181144 == 2) && (gGameSession->battleResetPending != 0)) {
            D_acropolis_promenade_80181144 = 0;
            func_800E8634(D_acropolis_promenade_80180F00, 0, D_acropolis_promenade_80181068);
        }
    }
}

/// Message gate for the promenade's three hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Message 0xA answers with the `warp` refusal code 1 while the disc has no
/// stream file open (`gDisplayState.debugMode < 0 || D_8006AC30.sector == 0`) or nibble 1 is
/// not yet at 4; the first pass at 4 advances it to 5 instead of refusing.
/// Message 0xC, while nibble 2 is still 0, refuses with code 3, latches the
/// answered record into `D_acropolis_promenade_801862D0` for the room's own
/// script to pick up, and arms `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent` with 4. Message 0xE spawns the
/// capsule sequence the first time (nibble 2 still 0) and afterwards reports
/// through `room` whether nibble 2 has reached 3.
///
/// `queryOnly` non-zero means "report only", which suppresses every side effect.
s32 func_acropolis_promenade_8017D70C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    u16          msgId;

    *out = *in;
    if (in->areaId == 0xA && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || D_8006AC30.sector == 0) {
            out->warp = 1;
        }
        if (GameFlag_GetNibble(1) == 4) {
            GameFlag_SetNibble(1, 5);
        } else {
            out->warp = 1;
        }
    }
    if (in->areaId == 0xC && GameFlag_GetNibble(2) == 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->warp                                           = 3;
            D_acropolis_promenade_801862D0                      = *out;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 4;
        }
        return 1;
    }
    msgId = in->areaId;
    if (msgId == 0xE) {
        if (GameFlag_GetNibble(2) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SpawnIfCapIdle(2, 1);
                Gp_SetNibbleIf(in->flagId, 2);
            }
            return 0;
        }
        if (in->areaId == msgId) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(2) == 3) {
                    out->room = 2;
                } else {
                    out->room = 1;
                }
            }
        }
    }
    return 1;
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_acropolis_promenade_8017D8D8(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_acropolis_promenade_8017D8E0(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 5) {
        if (Gp_GetCurBit2Flag(0x15) != 2) {
            Gp_StartCapSlot(5, 1, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_acropolis_promenade_8017D930(void)
{
}

s32 func_acropolis_promenade_8017D938(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 0xA:
            SndEvt_EnqueueType6(0x510B0009, 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(0x510B000A, 0, 0);
            break;
    }
    return 0;
}

/// State table of the room task, run by `func_acropolis_promenade_8017DA4C`.
static const TaskFuncTable3 D_acropolis_promenade_8017D5C4 = {
    { func_acropolis_promenade_8017D9E0, func_acropolis_promenade_8017D5E4, taskKill },
};

/// State table of the prop task, run by `func_acropolis_promenade_8017D988`.
static const TaskFuncTable3 D_acropolis_promenade_8017D5D0 = {
    { func_acropolis_promenade_8017DAA4, func_acropolis_promenade_8017DB48, taskKill },
};

/// Runs the prop task's current state (`func_acropolis_promenade_8017DAA4`,
/// `func_acropolis_promenade_8017DB48`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017D988(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5D0;
    sp.funcs[task->state](task);
}

static void func_acropolis_promenade_8017D9E0(Task* arg0)
{
    arg0->msgTable = D_acropolis_promenade_80180E74;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_promenade_801862D8 = Task_SpawnFromTable(D_acropolis_promenade_80180EA4, 0, 0, 0);
    arg0->state                    = (s32)(arg0->state + 1);
    D_80115598                     = 1;
}

/// Runs the room task's current state (`func_acropolis_promenade_8017D9E0`,
/// `func_acropolis_promenade_8017D5E4`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017DA4C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5C4;
    sp.funcs[task->state](task);
}

/// First state of the prop task whose model sits under the room's view
/// coordinate system (`D_acropolis_promenade_8017D5D0`): allocates its work
/// block, shows the model, places it at (-0x23F0, 0x12C, -0xAF0) under
/// `gGfxViewCoord` and moves to the next state.
static void func_acropolis_promenade_8017DAA4(Task* task)
{
    TmdObject*   extra;
    GfxCoord*    coord;
    ApmPropWork* work;

    extra = task->extra.tmd;
    coord = extra->coords;
    work  = memCalloc(sizeof(ApmPropWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work          = work;
    work->field_0       = 0;
    extra->flags        = 0;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = -0x23F0;
    coord->coord.t[1]   = 0x12C;
    coord->coord.t[2]   = -0xAF0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state++;
}

static void func_acropolis_promenade_8017DB48(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (Gp_GetViewIndex() == 5) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// The promenade's streamed-scene task. State 0 allocates the `RoomStreamWork`
/// block, cues the stream (slot-6 msg 0xFA4), captures slot 3 and the player's
/// coordinate matrix in the block, and republishes the player's weapon to slot
/// 3 with a 0x3E8 record. State 1 waits for the stream to come up
/// (`gCdCmdQueue::movieReady`), then starts the script pair and reparents this
/// task under it. State 2 drives the ride: every frame it moves the player's
/// matrix to the path entry the stream's countdown selects, offers the pad
/// prompt once (`Pad_CheckFlag800`, entry 3 of the room's task table) and, when
/// the prompt task reports back, warps slot 3 with a 0x3E9 placement and spawns
/// entry 4 instead; once the countdown is within 6 frames of the end it sends
/// the same placement as a 0x3F2 and moves on either way. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), stops the stream
/// (0xFA5), records the room in the save and kills the task.
void func_acropolis_promenade_8017DB9C(Task* task)
{
    AnimationPlayRequest rec;
    ActorTransform       place;
    s32                  killed;
    RoomStreamWork*      work;
    RoomStreamWork*      blk;
    RoomStreamWork*      dest;
    CdCmdQueue*          queue;
    s32                  weaponId;

    queue = &gCdCmdQueue;
    work  = (RoomStreamWork*)task->work;
    switch (task->state) {
        case 0:
            blk        = memCalloc(0x14, 0);
            task->work = blk;
            if (blk == NULL) {
                taskKill(task);
                break;
            }
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA4, 0, 0);
            ((RoomStreamWork*)task->work)->target = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomStreamWork*)task->work)->mtx    = gPlayerStatus.coordMtx;
            weaponId                              = gPlayerStatus.weapon;
            rec.source.index                      = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId                       = 1;
            rec.blend                             = ANIMATION_BLEND_RESET;
            rec.blendFrames                       = 0;
            rec.enableWorldCollision              = ANIMATION_WORLD_COLLISION_DISABLE;
            Gp_DispatchMsgPtr(((RoomStreamWork*)task->work)->target, ANIMATION_MESSAGE_PLAY, &rec, 0);
            func_800E9BDC(3, 0x9FF);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->script                  = Gp_SpawnScript18(D_acropolis_promenade_80186224,
                                                                 D_acropolis_promenade_8018623C);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->mtx->t[0] = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vx;
            work->mtx->t[1] = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vy;
            work->mtx->t[2] = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vz - 0xC8;
            if (work->spawned != 0) {
                if (Task_PollKill(work->child, &killed) != 0) {
                    place.pos.vx = 0x282;
                    place.pos.vy = 0x29;
                    place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vz - 0xC8;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    dest         = (RoomStreamWork*)task->work;
                    Gp_DispatchMsgPtr(dest->target, 0x3E9, &place, 0);
                    Task_SpawnFromTable(D_acropolis_promenade_80181148, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->child   = Task_SpawnFromTable(D_acropolis_promenade_80181148, 3, 0, 0);
                work->spawned = 1;
            }
            if ((0x45 - queue->movieFrame) < 6) {
                place.pos.vx = 0x282;
                place.pos.vy = 0x29;
                place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->movieFrame].vz - 0xC8;
                dest         = (RoomStreamWork*)task->work;
                Gp_DispatchMsgPtr(dest->target, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), 0xFA5, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
                func_800E9BDC(2, 0x9FF);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

/// Entry 3 of the room's task table: draws `Fade_DrawOverlay` at the grey
/// level `killCountdown`, which rises by 0x20 a frame; at 0x100 the task asks
/// to be killed with `Task_RequestKill`, which the streamed-scene task that
/// spawned it polls for.
void func_acropolis_promenade_8017DF74(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = (u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        Task_RequestKill(arg0, 0);
    }
}

/// Entry 4 of the room's task table: the reverse ramp of entry 3, drawing the
/// overlay at `~killCountdown`, and killing itself outright at the end.
void func_acropolis_promenade_8017DFD4(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = ~(u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

/// Per-frame effect spawner for the promenade. `D_acropolis_promenade_80181B74`
/// / `_80181B76` and the twelve-entry mask table `_80181B78` are per-view bit
/// masks: bit `view - 1` of an entry says whether that emitter is visible from
/// the camera `Gp_GetViewIndex` reports, and the parallel twelve-entry
/// `_80181B14` array holds each emitter's offset from the room's coordinate
/// frame. View 7 spawns nothing.
void func_acropolis_promenade_8017E03C(Task* task)
{
    GfxCoord*   coord;
    EffectWork* work;
    u8          view;
    s32         i;
    s32         mask;
    s16         prev;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    view  = Gp_GetViewIndex();
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    work->age++;
    if (view == 7) {
        return;
    }
    mask = 1 << (view - 1);
    if (D_acropolis_promenade_80181B74 & mask) {
        Gp_SpawnEff(0x8006004B, coord, (s32)(work->age), &D_acropolis_promenade_80181AFC[0]);
        Gp_SpawnEff(0x8006004B, coord, (s32)(work->age), &D_acropolis_promenade_80181AFC[1]);
        Gp_SpawnEff(0x60057, coord, (s32)(work->age), &D_acropolis_promenade_80181B0C[0]);
        glowDrawTintedDiscNoBias(&D_acropolis_promenade_80181AFC[-1], 0x100, 0x5C40);
    }
    for (i = 0; i < 3; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 0, &D_acropolis_promenade_80181B14[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i]);
        }
        if (D_acropolis_promenade_80181B78[i + 2] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 2]);
        }
        if (D_acropolis_promenade_80181B78[i + 4] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i + 4]);
        }
        if (D_acropolis_promenade_80181B78[i + 6] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 6]);
        }
    }
    if (D_acropolis_promenade_80181B78[11] & mask) {
        Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[11]);
    }
    if (D_acropolis_promenade_80181B76 & mask) {
        prev = work->scale;
        if (prev != view) {
            for (i = 0; i < 0x28; i++) {
                Gp_SpawnEff(0x60056, coord, (s32)(view), NULL);
            }
        } else {
            Gp_SpawnEff(0x60056, coord, (s32)(prev), NULL);
            Gp_SpawnEff(0x60056, coord, (s32)(prev), NULL);
        }
    }
    work->scale = view;
}

/// One falling water drip on the promenade, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole drip out of `gRandomLcgState`: `move.vx` is the column
/// (0..0xEF), `move.vy` the row it starts on (0xB0..0xEF), `scale` the
/// lifetime in frames, `angle` the width and `period` the number of frames
/// each row of fall takes. `gDisplayState.drawBuffer` picks the buffer half, and
/// the OT slot is the row scaled into the 0x500-deep range so a drip sorts
/// against the room behind it. The task releases itself once the camera turns
/// away, the lifetime runs out, or the drip falls off the bottom of the screen.
void func_acropolis_promenade_8017E394(Task* task)
{
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    mv;
    u16         rnd;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         onScreen;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1.value) {
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = (gRandomLcgState >> 16) % 240;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ((gRandomLcgState >> 16) & 0x3F) + 0xB0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->scale     = (u32)rnd % 90 + 0x1E;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            task->state++;
        }
        y        = work->move.vy + work->age / work->period;
        x        = work->move.vx;
        depth    = 0x500 - (y - 0xB0) * 10;
        onScreen = y < 0xEF;
        if (onScreen) {
            rect.x         = x;
            rect.y         = y + bufferY;
            rect.w         = work->angle;
            rect.h         = 1;
            mv             = gGpuPrimCursor;
            gGpuPrimCursor = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && onScreen) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One frame of the promenade's twinkling star: two semi-transparent
/// `POLY_FT4`s stacked on the same screen point, centred on the task's own
/// coordinate frame. The frame's translation is projected through `GsWSMATRIX`
/// into a 0x18-byte scratch stack block, and both quads are dropped
/// entirely inside `otz` 0x11.
///
/// The lower quad is upright, of half-extent `0x1680 / otz`, and animates
/// through six 0x10x0x10 cells at v = 0 on tpage 0x2B by stepping `u` with
/// `work->age % 6`; it is drawn `code |= 3`, so semi-transparent *and*
/// unshaded. The upper quad is the 0x27x0x27 flare at v = 0x10 with clut
/// 0x4381, drawn at `0x3A80 / otz` from the centre along the spin angle
/// `work->scale` and its quarter-turn (`+ 0x400`), so it rotates a frame at
/// a time. Its colour is a fresh random grey (0x20..0x7F, equal on all three
/// channels) every frame, which is what makes the star flicker.
///
/// Like the promenade's other glows, the task is one-shot: the work block is
/// released as soon as both quads have been queued, so the room respawns it
/// every frame it wants the star.
void func_acropolis_promenade_8017E634(Task* task)
{
    GfxCoord*             coord;
    EffectWork*           work;
    void**                scratch;
    u8*                   head;
    OverlaySpriteScratch* blk;
    s32*                  otzp;
    POLY_FT4*             prim;
    s32                   grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    work->age   = task->spawnArg1.value;
    scratch     = SCRATCH_STACK_CURSOR_SLOT;
    head        = *scratch;
    blk         = (OverlaySpriteScratch*)(head - 0x18);
    otzp        = &blk->otz;
    blk->vec.vx = coord->workm.t[0];
    blk->vec.vy = coord->workm.t[1];
    *scratch    = blk;
    blk->vec.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->vec);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(otzp);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4380;
        prim->code |= 3;
        prim->u0    = (work->age % 6) * 16;
        prim->v0    = 0;
        prim->u1    = (work->age % 6) * 16 + 0xF;
        prim->v1    = 0;
        prim->u2    = (work->age % 6) * 16;
        prim->v2    = 0xF;
        prim->u3    = (work->age % 6) * 16 + 0xF;
        prim->v3    = 0xF;
        blk->dx     = 0x1680 / blk->otz;
        blk->dy     = 0x1680 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->dx;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->dx;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->dy;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);

        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->clut      = 0x4381;
        prim->tpage     = 0x2B;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) % 96 + 0x20;
        prim->u0        = 0;
        prim->v0        = 0x10;
        prim->u1        = 0x27;
        prim->v1        = 0x10;
        prim->u2        = 0;
        prim->v2        = 0x37;
        prim->u3        = 0x27;
        prim->v3        = 0x37;
        prim->code     |= 2;
        prim->r0        = grey;
        prim->g0        = grey;
        prim->b0        = grey;

        work->scale = gDisplayState.animFrame + work->age;
        blk->dx     = ((0x3A80 / blk->otz) * rsin(work->scale)) >> 12;
        blk->dy     = ((0x3A80 / blk->otz) * rcos(work->scale)) >> 12;
        prim->x0    = blk->sxy.vx + blk->dx;
        prim->x3    = blk->sxy.vx - blk->dx;
        prim->y0    = blk->sxy.vy - blk->dy;
        prim->y3    = blk->sxy.vy + blk->dy;
        blk->dx     = ((0x3A80 / blk->otz) * rsin(work->scale + 0x400)) >> 12;
        blk->dy     = ((0x3A80 / blk->otz) * rcos(work->scale + 0x400)) >> 12;
        prim->x1    = blk->sxy.vx + blk->dx;
        prim->x2    = blk->sxy.vx - blk->dx;
        prim->y1    = blk->sxy.vy - blk->dy;
        prim->y2    = blk->sxy.vy + blk->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
    Gp_ReleaseState1CMem(work, task);
}

/// Draws one frame of the promenade's ground glow: a semi-transparent textured
/// quad lying flat under the task's coordinate frame. The four corner signs in
/// `D_acropolis_promenade_80181AE4` are scaled to +/-0x300 in `vx` / `vz` (with
/// `vy` left at zero, so the quad is horizontal), rotated by the task's own
/// `workm`, offset by that matrix's translation and then projected through
/// `GsWSMATRIX` into an `RoomQuadScratch` block taken from the scratch stack. The
/// first corner goes through `rtps` and the other three through `rtpt`, the
/// same split the sanctuary's mosaic tiles use.
///
/// The depth is biased by 0x20 before the near-plane test, so the glow survives
/// a little closer to the camera than the 0x11 cutoff alone would allow. Its
/// colour is a fresh random grey (0..0xF, equal on all three channels) every
/// frame, which is what makes it flicker; the quad is drawn semi-transparent
/// (`code |= 2`) from the 0x27x0x27 patch at v = 0x10 on tpage 0x2B.
///
/// The task is one-shot: the work block is released as soon as the quad has
/// been queued, so the room respawns it each frame it wants the glow.
void func_acropolis_promenade_8017ED44(Task* task)
{
    GfxCoord*        coord;
    EffectWork*      work;
    void**           scratch;
    u8*              head;
    RoomQuadScratch* blk;
    POLY_FT4*        prim;
    SVECTOR*         sv;
    s32              i;
    s32              grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    scratch   = SCRATCH_STACK_CURSOR_SLOT;
    head      = *scratch;
    work->age = task->spawnArg1.value;
    *scratch  = head - 0x24;
    blk       = (RoomQuadScratch*)(head - 0x24);
    for (i = 0; i < 4; i++) {
        blk->v[i].vx = D_acropolis_promenade_80181AE4[i].x * 0x300;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = D_acropolis_promenade_80181AE4[i].y * 0x300;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    prim->u0 = 0;
    prim->v0 = 0x10;
    prim->u1 = 0x27;
    prim->v1 = 0x10;
    prim->u2 = 0;
    prim->v2 = 0x37;
    prim->u3 = 0x27;
    prim->v3 = 0x37;
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    blk->otz += 0x20;
    if (blk->otz >= 0x11) {
        prim->tpage     = 0x2B;
        prim->clut      = 0x4381;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) & 0xF;
        prim->r0        = grey;
        prim->g0        = grey;
        prim->b0        = grey;
        prim->code     |= 2;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x24);
    Gp_ReleaseState1CMem(work, task);
}

/// Glow sprite task: queues one camera-facing, semi-transparent `POLY_FT4`
/// centred on the task's coordinate frame. The frame's translation is
/// projected through `GsWSMATRIX` into a 0x14-byte scratch stack block, and
/// the quad is a square of half-extent `0x6180 / otz` around the projected
/// point, so it shrinks with distance; nothing is drawn at `otz` 0x10 or less.
///
/// `Task::spawnArg1` (0..2) selects the 0x27x0x27 texture cell at
/// `u = (arg + 1) * 0x28`, `v = 0x10` on tpage 0x2B, the clut
/// `0x4380 | ((arg + 2) & 0x3F)`, and the grey level: a base of
/// 0x20 / 0x60 / 0x20, plus 0x08 / 0x10 / 0x0C on odd
/// `gDisplayState.animFrame`s.
///
/// The work block in `spawnArg2` is released after the quad is queued, so
/// each spawn draws a single frame.
void func_acropolis_promenade_8017F0BC(Task* task)
{
    GfxCoord*              coord;
    EffectWork*            work;
    RoomGlowSpriteScratch* blk;
    POLY_FT4*              prim;
    s32                    grey;
    s32                    clut;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    blk         = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    blk->pos.vx = coord->workm.t[0];
    blk->pos.vy = coord->workm.t[1];
    blk->pos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->pos);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        u8 base[3] = { 0x20, 0x60, 0x20 };
        u8 step[3] = { 0x08, 0x10, 0x0C };

        grey        = base[task->spawnArg1.value] + (gDisplayState.animFrame & 1) * step[task->spawnArg1.value];
        prim->code |= 2;
        prim->tpage = 0x2B;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        // Assigning through an `s32` keeps the load of `spawnArg1` in SImode;
        // storing the expression straight into the `u16` field lets the front
        // end shorten the whole chain and the load becomes an `lhu`.
        clut       = ((task->spawnArg1.value + 2) & 0x3F) | 0x4380;
        prim->clut = clut;
        prim->u0   = (task->spawnArg1.value + 1) * 0x28;
        prim->v0   = 0x10;
        prim->u1   = (task->spawnArg1.value + 1) * 0x28 + 0x27;
        prim->v1   = 0x10;
        prim->u2   = (task->spawnArg1.value + 1) * 0x28;
        prim->v2   = 0x37;
        prim->u3   = (task->spawnArg1.value + 1) * 0x28 + 0x27;
        prim->v3   = 0x37;
        blk->half  = 0x6180 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->half;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->half;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->half;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->half;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    Gp_ReleaseState1CMem(work, task);
}

#include "../../shared/glow_draw_tinted_disc_no_bias.inc.c"
