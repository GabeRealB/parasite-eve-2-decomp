#include "rooms/acropolis_forked_road.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_forked_road_private.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
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
#include "main/gameflow.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern SVECTOR D_acropolis_forked_road_80182204[2];

/// Set to 1 by the fade-out task once the scene has finished.

/// Per-frame path the streamed scene walks `Player_Status.coordMtx` along, indexed by
/// `CdCmd_Queue::field_1EA - 1` for the 0x78 frames the ride lasts.
extern SVECTOR D_acropolis_forked_road_80180F80[];

/// The script pair the streamed scene runs.
extern GpScriptCmd D_acropolis_forked_road_80185058[6];
extern GpScriptRec D_acropolis_forked_road_80185070[2];

/// The script pair the return ride runs.
extern GpScriptCmd D_acropolis_forked_road_80185038[6];
extern GpScriptRec D_acropolis_forked_road_80185050[2];

/// The fourteen spawn offsets of the forked road's ambient effects, indexed
/// 0..13 by the first-frame burst below.
extern SVECTOR D_acropolis_forked_road_80182178[14];

/// One bit per in-game day (shifted by `GameSession::at4.loc.view - 1`) for each of
/// the sixteen ambient-effect slots: which of the room's lamps are lit today.
extern u16 D_acropolis_forked_road_801821E8[14];

/// The two points the twin trail is anchored at, relative to its parent frame:
/// `[0]` places the task's own frame and `[1]`, also reached by its own name,
/// the second trail's.

static void func_acropolis_forked_road_8017EC70(GfxCoord* coord, s32 arg1, s16 arg2);
static void func_acropolis_forked_road_8017F224(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_acropolis_forked_road_8017F650(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_acropolis_forked_road_8017FED4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_acropolis_forked_road_80180554(GfxCoord* arg0, s16 arg1, u8* arg2);

void func_acropolis_forked_road_8017DA24(Task*);
void func_acropolis_forked_road_8017DD60(Task*);
void func_acropolis_forked_road_8017E1C0(Task*);
void func_acropolis_forked_road_8017E220(Task*);

extern AnimationPlayRequest D_acropolis_forked_road_8018207C;
extern GpCopyArg            D_acropolis_forked_road_80182060;
extern GpGridParams         D_acropolis_forked_road_80182BF0[1];
extern GpObj4C              D_acropolis_forked_road_80182C14[6];
extern GpObj4C              D_acropolis_forked_road_80182DDC[7];
extern GpRoomCoordSet       D_acropolis_forked_road_80184E70[1];
void                        func_acropolis_forked_road_8017E288(void);

extern GpSprtCmd  D_acropolis_forked_road_80183284[2];
extern GpSprtCmd  D_acropolis_forked_road_80183938[12];
extern GpSprtCmd  D_acropolis_forked_road_80183A10[3];
extern GpSprtCmd  D_acropolis_forked_road_80183A28[2];
extern GpSprtCmd  D_acropolis_forked_road_80183AB0[3];
extern GpSprtCmd  D_acropolis_forked_road_80183AC8[2];
extern GpSprtCmd  D_acropolis_forked_road_80183F70[6];
extern GpSprtCmd  D_acropolis_forked_road_80183FA0[2];
extern GpSprtCmd  D_acropolis_forked_road_80183FB0[2];
extern GpSprtCmd  D_acropolis_forked_road_80184498[7];
extern GpSprtElem D_acropolis_forked_road_80183294[85];
extern GpSprtElem D_acropolis_forked_road_80183998[6];
extern GpSprtElem D_acropolis_forked_road_80183A38[6];
extern GpSprtElem D_acropolis_forked_road_80183AE8[58];
extern GpSprtElem D_acropolis_forked_road_80183FC0[62];

TaskDesc D_acropolis_forked_road_80180F44[5] = {
    { 0, 192, func_acropolis_forked_road_8017DA24, { .model = NULL } },
    { 0, 192, NULL, { .model = NULL } },
    { 0, 192, func_acropolis_forked_road_8017DD60, { .model = NULL } },
    { 0, 192, func_acropolis_forked_road_8017E1C0, { .model = NULL } },
    { 0, 192, func_acropolis_forked_road_8017E220, { .model = NULL } },
};

SVECTOR D_acropolis_forked_road_80180F80[300] = {
    { 1040, 5, 1250, 0 },
    { 1065, 5, 1250, 0 },
    { 1090, 5, 1250, 0 },
    { 1116, 5, 1250, 0 },
    { 1141, 5, 1250, 0 },
    { 1167, 5, 1250, 0 },
    { 1192, 5, 1250, 0 },
    { 1217, 5, 1250, 0 },
    { 1243, 5, 1250, 0 },
    { 1268, 5, 1250, 0 },
    { 1294, 5, 1250, 0 },
    { 1319, 5, 1250, 0 },
    { 1344, 2, 1250, 0 },
    { 1365, -11, 1250, 0 },
    { 1386, -26, 1250, 0 },
    { 1407, -40, 1250, 0 },
    { 1428, -54, 1250, 0 },
    { 1450, -68, 1250, 0 },
    { 1471, -82, 1250, 0 },
    { 1492, -96, 1250, 0 },
    { 1513, -110, 1250, 0 },
    { 1534, -124, 1250, 0 },
    { 1555, -138, 1250, 0 },
    { 1576, -152, 1250, 0 },
    { 1598, -167, 1250, 0 },
    { 1619, -181, 1250, 0 },
    { 1640, -195, 1250, 0 },
    { 1661, -209, 1250, 0 },
    { 1682, -223, 1250, 0 },
    { 1703, -237, 1250, 0 },
    { 1725, -251, 1250, 0 },
    { 1746, -265, 1250, 0 },
    { 1767, -279, 1250, 0 },
    { 1788, -294, 1250, 0 },
    { 1809, -308, 1250, 0 },
    { 1830, -322, 1250, 0 },
    { 1852, -336, 1250, 0 },
    { 1873, -350, 1250, 0 },
    { 1894, -364, 1250, 0 },
    { 1915, -378, 1250, 0 },
    { 1936, -392, 1250, 0 },
    { 1957, -406, 1250, 0 },
    { 1978, -420, 1250, 0 },
    { 2000, -435, 1250, 0 },
    { 2021, -449, 1250, 0 },
    { 2042, -463, 1250, 0 },
    { 2063, -477, 1250, 0 },
    { 2084, -491, 1250, 0 },
    { 2105, -505, 1250, 0 },
    { 2127, -519, 1250, 0 },
    { 2148, -533, 1250, 0 },
    { 2169, -547, 1250, 0 },
    { 2190, -562, 1250, 0 },
    { 2211, -576, 1250, 0 },
    { 2232, -590, 1250, 0 },
    { 2253, -604, 1250, 0 },
    { 2275, -618, 1250, 0 },
    { 2296, -632, 1250, 0 },
    { 2317, -646, 1250, 0 },
    { 2338, -660, 1250, 0 },
    { 2359, -674, 1250, 0 },
    { 2380, -688, 1250, 0 },
    { 2402, -703, 1250, 0 },
    { 2423, -717, 1250, 0 },
    { 2444, -731, 1250, 0 },
    { 2465, -745, 1250, 0 },
    { 2486, -759, 1250, 0 },
    { 2507, -773, 1250, 0 },
    { 2529, -787, 1250, 0 },
    { 2550, -801, 1250, 0 },
    { 2571, -815, 1250, 0 },
    { 2592, -829, 1250, 0 },
    { 2613, -844, 1250, 0 },
    { 2634, -858, 1250, 0 },
    { 2655, -872, 1250, 0 },
    { 2677, -886, 1250, 0 },
    { 2698, -900, 1250, 0 },
    { 2719, -914, 1250, 0 },
    { 2740, -928, 1250, 0 },
    { 2761, -942, 1250, 0 },
    { 2782, -956, 1250, 0 },
    { 2804, -971, 1250, 0 },
    { 2825, -985, 1250, 0 },
    { 2846, -999, 1250, 0 },
    { 2867, -1013, 1250, 0 },
    { 2888, -1027, 1250, 0 },
    { 2909, -1041, 1250, 0 },
    { 2930, -1055, 1250, 0 },
    { 2952, -1069, 1250, 0 },
    { 2973, -1083, 1250, 0 },
    { 2994, -1097, 1250, 0 },
    { 3015, -1112, 1250, 0 },
    { 3036, -1126, 1250, 0 },
    { 3057, -1140, 1250, 0 },
    { 3079, -1154, 1250, 0 },
    { 3100, -1168, 1250, 0 },
    { 3121, -1182, 1250, 0 },
    { 3142, -1196, 1250, 0 },
    { 3163, -1210, 1250, 0 },
    { 3184, -1224, 1250, 0 },
    { 3205, -1238, 1250, 0 },
    { 3227, -1253, 1250, 0 },
    { 3248, -1267, 1250, 0 },
    { 3269, -1281, 1250, 0 },
    { 3290, -1295, 1250, 0 },
    { 3311, -1309, 1250, 0 },
    { 3332, -1323, 1250, 0 },
    { 3354, -1337, 1250, 0 },
    { 3375, -1351, 1250, 0 },
    { 3396, -1365, 1250, 0 },
    { 3417, -1380, 1250, 0 },
    { 3438, -1394, 1250, 0 },
    { 3459, -1408, 1250, 0 },
    { 3481, -1422, 1250, 0 },
    { 3502, -1436, 1250, 0 },
    { 3523, -1450, 1250, 0 },
    { 3544, -1464, 1250, 0 },
    { 3565, -1478, 1250, 0 },
    { 3586, -1492, 1250, 0 },
    { 3607, -1506, 1250, 0 },
    { 3629, -1521, 1250, 0 },
    { 3650, -1535, 1250, 0 },
    { 3671, -1549, 1250, 0 },
    { 3692, -1563, 1250, 0 },
    { 3713, -1577, 1250, 0 },
    { 3734, -1591, 1250, 0 },
    { 3756, -1605, 1250, 0 },
    { 3777, -1619, 1250, 0 },
    { 3798, -1633, 1250, 0 },
    { 3819, -1648, 1250, 0 },
    { 3840, -1662, 1250, 0 },
    { 3861, -1676, 1250, 0 },
    { 3882, -1690, 1250, 0 },
    { 3904, -1704, 1250, 0 },
    { 3925, -1718, 1250, 0 },
    { 3946, -1732, 1250, 0 },
    { 3967, -1746, 1250, 0 },
    { 3988, -1760, 1250, 0 },
    { 4009, -1774, 1250, 0 },
    { 4031, -1789, 1250, 0 },
    { 4052, -1803, 1250, 0 },
    { 4073, -1817, 1250, 0 },
    { 4094, -1831, 1250, 0 },
    { 4115, -1845, 1250, 0 },
    { 4136, -1859, 1250, 0 },
    { 4158, -1873, 1250, 0 },
    { 4179, -1887, 1250, 0 },
    { 4200, -1901, 1250, 0 },
    { 4221, -1915, 1250, 0 },
    { 4242, -1930, 1250, 0 },
    { 4263, -1944, 1250, 0 },
    { 4284, -1958, 1250, 0 },
    { 4306, -1972, 1250, 0 },
    { 4327, -1986, 1250, 0 },
    { 4348, -2000, 1250, 0 },
    { 4369, -2014, 1250, 0 },
    { 4390, -2028, 1250, 0 },
    { 4411, -2042, 1250, 0 },
    { 4433, -2057, 1250, 0 },
    { 4454, -2071, 1250, 0 },
    { 4475, -2085, 1250, 0 },
    { 4496, -2099, 1250, 0 },
    { 4517, -2113, 1250, 0 },
    { 4538, -2127, 1250, 0 },
    { 4559, -2141, 1250, 0 },
    { 4581, -2155, 1250, 0 },
    { 4602, -2169, 1250, 0 },
    { 4623, -2183, 1250, 0 },
    { 4644, -2198, 1250, 0 },
    { 4665, -2212, 1250, 0 },
    { 4686, -2226, 1250, 0 },
    { 4708, -2240, 1250, 0 },
    { 4729, -2254, 1250, 0 },
    { 4750, -2268, 1250, 0 },
    { 4771, -2282, 1250, 0 },
    { 4792, -2296, 1250, 0 },
    { 4813, -2310, 1250, 0 },
    { 4835, -2325, 1250, 0 },
    { 4856, -2339, 1250, 0 },
    { 4877, -2353, 1250, 0 },
    { 4898, -2367, 1250, 0 },
    { 4919, -2381, 1250, 0 },
    { 4940, -2395, 1250, 0 },
    { 4961, -2409, 1250, 0 },
    { 4983, -2423, 1250, 0 },
    { 5004, -2437, 1250, 0 },
    { 5025, -2451, 1250, 0 },
    { 5046, -2466, 1250, 0 },
    { 5067, -2480, 1250, 0 },
    { 5088, -2494, 1250, 0 },
    { 5110, -2508, 1250, 0 },
    { 5131, -2522, 1250, 0 },
    { 5152, -2536, 1250, 0 },
    { 5173, -2550, 1250, 0 },
    { 5194, -2564, 1250, 0 },
    { 5215, -2578, 1250, 0 },
    { 5236, -2592, 1250, 0 },
    { 5258, -2607, 1250, 0 },
    { 5279, -2621, 1250, 0 },
    { 5300, -2635, 1250, 0 },
    { 5321, -2649, 1250, 0 },
    { 5342, -2663, 1250, 0 },
    { 5363, -2677, 1250, 0 },
    { 5385, -2691, 1250, 0 },
    { 5406, -2705, 1250, 0 },
    { 5427, -2719, 1250, 0 },
    { 5448, -2734, 1250, 0 },
    { 5469, -2748, 1250, 0 },
    { 5490, -2762, 1250, 0 },
    { 5512, -2776, 1250, 0 },
    { 5533, -2790, 1250, 0 },
    { 5554, -2804, 1250, 0 },
    { 5575, -2818, 1250, 0 },
    { 5596, -2832, 1250, 0 },
    { 5617, -2846, 1250, 0 },
    { 5638, -2860, 1250, 0 },
    { 5660, -2875, 1250, 0 },
    { 5681, -2889, 1250, 0 },
    { 5702, -2903, 1250, 0 },
    { 5723, -2917, 1250, 0 },
    { 5744, -2931, 1250, 0 },
    { 5765, -2945, 1250, 0 },
    { 5787, -2959, 1250, 0 },
    { 5808, -2973, 1250, 0 },
    { 5829, -2987, 1250, 0 },
    { 5852, -2995, 1250, 0 },
    { 5878, -2995, 1250, 0 },
    { 5903, -2995, 1250, 0 },
    { 5928, -2995, 1250, 0 },
    { 5954, -2995, 1250, 0 },
    { 5979, -2995, 1250, 0 },
    { 6005, -2995, 1250, 0 },
    { 6030, -2995, 1250, 0 },
    { 6056, -2995, 1250, 0 },
    { 6081, -2995, 1250, 0 },
    { 6106, -2995, 1250, 0 },
    { 6132, -2995, 1250, 0 },
    { 6157, -2995, 1250, 0 },
    { 6183, -2995, 1250, 0 },
    { 6208, -2995, 1250, 0 },
    { 6234, -2995, 1250, 0 },
    { 6259, -2995, 1250, 0 },
    { 6284, -2995, 1250, 0 },
    { 6310, -2995, 1250, 0 },
    { 6335, -2995, 1250, 0 },
    { 6361, -2995, 1250, 0 },
    { 6386, -2995, 1250, 0 },
    { 6412, -2995, 1250, 0 },
    { 6437, -2995, 1250, 0 },
    { 6462, -2995, 1250, 0 },
    { 6488, -2995, 1250, 0 },
    { 6513, -2995, 1250, 0 },
    { 6539, -2995, 1250, 0 },
    { 6564, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
    { 6590, -2995, 1250, 0 },
};

AnimationPackedPose D_acropolis_forked_road_801818E0[6] = {
#include "assets/acropolis_forked_road_animation_045FC_bank1.inc"
};

AnimationPackedRotation D_acropolis_forked_road_80181928[46] = {
#include "assets/acropolis_forked_road_animation_045FC_bank4.inc"
};

AnimationRecord D_acropolis_forked_road_801819E0[109] = {
#include "assets/acropolis_forked_road_animation_045FC_records.inc"
};

u16 D_acropolis_forked_road_80181B94[20] = {
#include "assets/acropolis_forked_road_animation_045FC_indices.inc"
};

GpAnimSet D_acropolis_forked_road_80181BBC = {
    D_acropolis_forked_road_801819E0,
    D_acropolis_forked_road_80181B94,
    { NULL, D_acropolis_forked_road_801818E0, NULL, NULL, D_acropolis_forked_road_80181928, NULL, NULL, NULL },
};

AnimationPackedPose D_acropolis_forked_road_80181BE4[10] = {
#include "assets/acropolis_forked_road_animation_04A6C_bank1.inc"
};

AnimationPackedRotation D_acropolis_forked_road_80181C5C[95] = {
#include "assets/acropolis_forked_road_animation_04A6C_bank4.inc"
};

AnimationRecord D_acropolis_forked_road_80181DD8[139] = {
#include "assets/acropolis_forked_road_animation_04A6C_records.inc"
};

u16 D_acropolis_forked_road_80182004[20] = {
#include "assets/acropolis_forked_road_animation_04A6C_indices.inc"

};

GpAnimSet D_acropolis_forked_road_8018202C = {
    D_acropolis_forked_road_80181DD8,
    D_acropolis_forked_road_80182004,
    { NULL, D_acropolis_forked_road_80181BE4, NULL, NULL, D_acropolis_forked_road_80181C5C, NULL, NULL, NULL },
};

GpAnimSet* D_acropolis_forked_road_80182054[3] = {
    NULL,
    &D_acropolis_forked_road_80181BBC,
    &D_acropolis_forked_road_8018202C,
};

GpCopyArg D_acropolis_forked_road_80182060 = { { .sets = D_acropolis_forked_road_80182054 }, 3 };

AnimationPlayRequest D_acropolis_forked_road_80182068 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_forked_road_8018207C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_forked_road_80182090[2] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
};

GpEvsCmd D_acropolis_forked_road_801820B8[8] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_acropolis_forked_road_80182060 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_forked_road_8018207C }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_forked_road_8017E288 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_acropolis_forked_road_80182178[14] = {
    { -50, -1810, 2070, 0 },
    { -50, -1810, 430, 0 },
    { 470, -3290, -2200, 0 },
    { 80, -3290, -3070, 0 },
    { -1480, -220, 2450, 0 },
    { -540, -220, 2410, 0 },
    { 280, -220, -50, 0 },
    { 270, -220, -1130, 0 },
    { -430, -220, -1800, 0 },
    { -1400, -220, -2310, 0 },
    { -2250, -220, -2770, 0 },
    { -3310, -220, -3330, 0 },
    { 1550, -1058, -790, 0 },
    { -770, -1058, -2990, 0 },
};

// Gameplay effect bank 6, entry 0x89 calls the lamp callback at 0x8017E410.
// The ambient spawner supplies exactly indices 0..13 through that shared table.
u16 D_acropolis_forked_road_801821E8[14] = {
    208,
    240,
    20,
    20,
    20,
    212,
    468,
    276,
    276,
    20,
    0,
    8,
    244,
    34,
};

SVECTOR D_acropolis_forked_road_80182204[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_acropolis_forked_road_80182214[3] = {
    { D_acropolis_forked_road_80182BF0, D_acropolis_forked_road_80182C14, D_acropolis_forked_road_80182DDC, NULL },
    { D_acropolis_forked_road_80182BF0, D_acropolis_forked_road_80182C14, D_acropolis_forked_road_80182DDC, NULL },
    { D_acropolis_forked_road_80182BF0, D_acropolis_forked_road_80182C14, D_acropolis_forked_road_80182DDC, NULL },
};

u8 D_acropolis_forked_road_80182244[12] = {
    1,
    2,
    5,
    4,
    5,
    6,
    8,
    8,
    9,
    0,
    0,
    0,
};

u8 D_acropolis_forked_road_80182250[12] = {
    1,
    2,
    5,
    4,
    5,
    6,
    8,
    8,
    9,
    0,
    0,
    0,
};

u8* D_acropolis_forked_road_8018225C[3] = {
    D_8010CAF8,
    D_acropolis_forked_road_80182244,
    D_acropolis_forked_road_80182250,
};

GpViewCountRec D_acropolis_forked_road_80182268[3] = {
    { { .bytes = { 10, 0 } } },
    { { .bytes = { 9, 0 } } },
    { { .bytes = { 9, 0 } } },
};

GpRoomCoordRec D_acropolis_forked_road_80182270[3] = {
    { D_acropolis_forked_road_80184E70, NULL },
    { D_acropolis_forked_road_80184E70, NULL },
    { D_acropolis_forked_road_80184E70, NULL },
};

GpWarpRec D_acropolis_forked_road_80182288[5] = {
    { { .words = { 1920, -3791, 1, 833 } }, { 0, 0, 0, 0 }, { .words = { 1920, -3791, 1, 833 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 3072, -261, 1, 1205 } }, { 0, 0, 0, 0 }, { .words = { 3072, -261, 1, 1205 } }, { 0, 0, 0, 0 }, 0, 0, 0, 8, 0, 494 },
    { { .words = { 0, -5107, 1, -4030 } }, { 0, 0, 0, 0 }, { .words = { 0, -5107, 1, -4030 } }, { 0, 0, 0, 0 }, 0x51090003, 0x51090002, 0x51090006, 4, 0, 495 },
    { { .words = { 1792, -4094, -470, 1325 } }, { 0, 0, 0, 0 }, { .words = { 1792, -3764, -600, 797 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 1, 0 },
    { { .words = { 3072, -261, 1, 1205 } }, { 0, 0, 0, 0 }, { .words = { 3072, -261, 1, 1205 } }, { 0, 0, 0, 0 }, 0, 0, 0, 3, 0, 494 },
};

SVECTOR D_acropolis_forked_road_801823A0[18] = {
#include "assets/acropolis_forked_road_collision_05630_normals.inc"
};

SVECTOR D_acropolis_forked_road_80182430[118] = {
#include "assets/acropolis_forked_road_collision_05630_verts.inc"
};

GpGridFace D_acropolis_forked_road_801827E0[38] = {
#include "assets/acropolis_forked_road_collision_05630_faces.inc"
};

s16 D_acropolis_forked_road_801829A8[244] = {
#include "assets/acropolis_forked_road_collision_05630_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_forked_road_801829A8[i])
s16* D_acropolis_forked_road_80182B90[24] = {
#include "assets/acropolis_forked_road_collision_05630_table.inc"
};
#undef GRID_CELL

GpGridParams D_acropolis_forked_road_80182BF0[1] = {
    { NULL, D_acropolis_forked_road_801823A0, D_acropolis_forked_road_80182430, D_acropolis_forked_road_801827E0, D_acropolis_forked_road_80182B90, 0x303E, 6900, 6, 4, 4000, 38 },
};

GpObj4C D_acropolis_forked_road_80182C14[6] = {
    { NULL, NULL, NULL, { -1905, 0, -353, 0 }, { { -509, 3168, 2239, 0 }, { 510, 3168, -2238, 0 }, { -509, -3168, 2239, 0 }, { 510, -3168, -2238, 0 } }, { 3996, 0, 909, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -1712, 0, -149, 0 }, { { 526, 3168, -2286, 0 }, { -525, 3168, 2287, 0 }, { 526, -3168, -2286, 0 }, { -525, -3168, 2287, 0 } }, { -4002, 0, -920, 0 }, { 0, 0, 4096, 0 }, 3941, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -6711, 0, -2121, 0 }, { { 1938, 3168, 482, 0 }, { -1937, 3168, -481, 0 }, { 1938, -3168, 482, 0 }, { -1937, -3168, -481, 0 } }, { 989, 0, -3983, 0 }, { 0, 0, 4096, 0 }, 3736, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { -6668, 0, -2514, 0 }, { { -1920, 3168, -512, 0 }, { 1920, 3168, 513, 0 }, { -1920, -3168, -512, 0 }, { 1920, -3168, 513, 0 } }, { -1058, 0, 3959, 0 }, { 0, 0, 4096, 0 }, 3736, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { -3905, 0, -3010, 0 }, { { -943, 3168, 1067, 0 }, { 943, 3168, -1066, 0 }, { -943, -3168, 1067, 0 }, { 943, -3168, -1066, 0 } }, { 3088, 0, 2731, 0 }, { 0, 0, 4096, 0 }, 3472, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { -3575, 0, -2822, 0 }, { { 1290, 3168, -1366, 0 }, { -1290, 3168, 1367, 0 }, { 1290, -3168, -1366, 0 }, { -1290, -3168, 1367, 0 } }, { -2990, 0, -2822, 0 }, { 0, 0, 4096, 0 }, 3683, 0, 4, 2, 129, 0 },
};

GpObj4C D_acropolis_forked_road_80182DDC[7] = {
    { NULL, NULL, NULL, { -5024, -64, -4192, 0 }, { { -896, 0, -768, 0 }, { 896, 0, -768, 0 }, { -896, 0, 768, 0 }, { 896, 0, 768, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1180, 0, 8, 50, 2, 0 },
    { NULL, NULL, NULL, { -192, -96, 1440, 0 }, { { -768, 0, -736, 0 }, { 768, 0, -736, 0 }, { -768, 0, 736, 0 }, { 768, 0, 736, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1063, 0, 10, 36, 2, 0 },
    { NULL, NULL, NULL, { -3905, -128, 1039, 0 }, { { -433, 0, -633, 0 }, { 721, 0, -218, 0 }, { -720, 0, 219, 0 }, { 434, 0, 634, 0 } }, { 0, 4114, 0, 0 }, { 1189, 0, -3920, 0 }, 768, 1, 68, 244, 2, 0 },
    { NULL, NULL, NULL, { 1049, -96, -712, 0 }, { { -1285, 0, 644, 0 }, { -1287, 0, -1365, 0 }, { 232, 0, 613, 0 }, { 231, 0, -468, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1872, 6, 4, 0, 4, 0 },
    { NULL, NULL, NULL, { -3922, -656, 1150, 0 }, { { -1191, -1520, -451, 0 }, { 1192, -1520, 452, 0 }, { -1191, 1520, -451, 0 }, { 1192, 1520, 452, 0 } }, { 1451, 0, -3834, 0 }, { 1380, 0, -3857, 0 }, 1982, 0x8000, 1, 22, 2, 0 },
    { NULL, NULL, NULL, { -4113, -512, 1439, 0 }, { { -983, 0, -519, 0 }, { 1087, 0, 270, 0 }, { -1087, 0, -270, 0 }, { 983, 0, 519, 0 } }, { 0, 4098, 0, 0 }, { -1568, 0, 3784, 0 }, 1115, 0x8101, 66, 112, 2, 0 },
    { NULL, NULL, NULL, { -2336, -96, -512, 0 }, { { -256, 0, -2624, 0 }, { 1344, 0, -1600, 0 }, { -1344, 0, 1600, 0 }, { 256, 0, 2624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 2635, 0x8005, 1, 0, 131, 0 },
};

GpAreaTmdRec D_acropolis_forked_road_80182FF0[2] = {
    { 19, 19, 2, 0, { 0, 0 }, D_80179120 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183008[3] = {
    { 10, 10, 3, 0, { 0, 0 }, D_80155004 },
    { 7, 7, 2, 0, { 0, 0 }, D_801693AC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_8018302C[2] = {
    { 55, 55, 0, 0, { 0, 0 }, D_8013A8DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183044[3] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 7, 7, 1, 0, { 0, 0 }, D_80150C80 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183068[2] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183080[2] = {
    { 18, 18, 3, 0, { 0, 0 }, D_80155AC4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183098[3] = {
    { 7, 7, 0, 0, { 0, 0 }, D_80138C80 },
    { 8, 7, 0, 0, { 0, 0 }, D_801393C8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_801830BC[2] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_801830D4[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_801830EC[2] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183104[2] = {
    { 37, 37, 0, 0, { 0, 0 }, D_80139DAC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_8018311C[2] = {
    { 15, 15, 0, 0, { 0, 0 }, D_8013BE28 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183134[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_8018314C[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183164[3] = {
    { 46, 46, 0, 0, { 0, 0 }, D_80137698 },
    { 47, 47, 0, 0, { 0, 0 }, D_801382BC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_forked_road_80183188[5] = {
    { 70, 70, 0, 0, { 0, 0 }, D_8013F5F0 },
    { 71, 71, 0, 0, { 0, 0 }, D_80139E60 },
    { 72, 72, 1, 0, { 0, 0 }, D_80153EC8 },
    { 73, 73, 1, 0, { 0, 0 }, D_8014E7A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_acropolis_forked_road_801831C4[24] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B1DC, D_acropolis_forked_road_80182FF0 },
    { D_map_akropolis_8017B1FC, D_acropolis_forked_road_80183008 },
    { D_map_akropolis_8017B24C, D_acropolis_forked_road_8018302C },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B29C, D_acropolis_forked_road_80183044 },
    { D_map_akropolis_8017B2EC, D_acropolis_forked_road_80183068 },
    { D_map_akropolis_8017B33C, D_acropolis_forked_road_80183080 },
    { D_map_akropolis_8017B35C, D_acropolis_forked_road_80183098 },
    { D_map_akropolis_8017B41C, D_acropolis_forked_road_801830BC },
    { D_map_akropolis_8017B48C, D_acropolis_forked_road_801830D4 },
    { D_map_akropolis_8017B4FC, D_acropolis_forked_road_801830EC },
    { D_map_akropolis_8017B54C, D_acropolis_forked_road_80183104 },
    { D_map_akropolis_8017B5BC, D_acropolis_forked_road_8018311C },
    { D_map_akropolis_8017B5EC, D_acropolis_forked_road_80183134 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B65C, D_acropolis_forked_road_8018314C },
    { D_map_akropolis_8017B67C, D_acropolis_forked_road_80183164 },
    { D_map_akropolis_8017B6AC, D_acropolis_forked_road_80183188 },
    { NULL, NULL },
};

GpSprtCmd D_acropolis_forked_road_80183284[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_forked_road_80183294[85] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -32, 1212, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -32, 1237, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, -56, 1237, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -80, 1237, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -72, 1222, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -56, 1222, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, -72, 1212, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, -56, 1212, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -24, 1350, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 16, 1375, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 56, 1375, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 16, 1400, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -24, 1375, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -24, 1400, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 16, 1400, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 40, 1425, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -40, 1312, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, -64, 1312, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -64, 1375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, -32, 1375, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 40, 400, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 400, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 400, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, 48, 400, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -120, 48, 400, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 32, 400, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, 48, 400, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 56, 450, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 80, 450, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 104, 450, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 80, 450, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 104, 450, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 450, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 450, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -112, 80, 450, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 64, 450, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 64, 450, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, 48, 450, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -40, 48, 450, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 64, 450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 96, 450, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 120, 104, 425, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 88, 88, 425, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, 96, 425, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, 104, 425, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 425, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -136, 80, 455, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -136, 56, 455, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -144, 64, 455, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 80, 455, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, 64, 500, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 48, 700, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 56, 650, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -160, 72, 600, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 500, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 56, 650, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 72, 600, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -128, 88, 500, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -96, 72, 600, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -96, 88, 500, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 88, 500, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 80, 600, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -32, 96, 500, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, 104, 500, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 112, 500, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 72, 1050, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 40, 1050, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -160, 8, 1050, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -24, 1050, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -56, 1050, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -88, 1050, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, -104, 1050, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 0, 2750, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 24, 2475, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -120, 24, 2475, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -80, 24, 2475, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -40, 24, 2475, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 8, 2500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 8, 2500, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 8, 2500, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 8, 2500, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 0, 2750, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 0, 24, 2475, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 8, 2500, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 0, 2750, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_forked_road_80183938[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 6, 0 } },
    { 8, 8, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 8, 0 } },
    { 20, 7, 0, 0, { 0, 0 } },
    { 27, 14, 0, 0, { 5, 0 } },
    { 41, 5, 0, 0, { 2, 0 } },
    { 46, 4, 0, 0, { 9, 0 } },
    { 50, 15, 0, 0, { 4, 0 } },
    { 65, 7, 0, 0, { 7, 0 } },
    { 72, 13, 0, 0, { 3, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_forked_road_80183998[6] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 937, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 937, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 937, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -88, 937, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -24, 937, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_forked_road_80183A10[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_forked_road_80183A28[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_forked_road_80183A38[6] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 937, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 937, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 937, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -88, 937, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -24, 937, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_forked_road_80183AB0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_forked_road_80183AC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_forked_road_80183AD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_forked_road_80183AE8[58] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -96, 750, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, -104, 750, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, -88, 750, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -40, -88, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, -88, 750, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, -88, 750, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -88, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, -56, 750, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, -56, 750, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, -56, 750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, -56, 750, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -56, 750, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -40, -120, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, -120, 750, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, -120, 750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -40, 1125, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -40, 1125, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -64, 1125, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -64, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -80, 1125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -80, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -80, 1125, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, -72, 1125, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -64, 1125, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, -72, 1125, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -64, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -40, -88, 1178, { .fields = { 40, 184 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 32, -88, 1178, { .fields = { 40, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, -80, 1185, { .fields = { 40, 168 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -24, -80, 1197, { .fields = { 64, 232 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 16, -80, 1197, { .fields = { 48, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -80, 1195, { .fields = { 40, 176 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, -72, 1195, { .fields = { 40, 200 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -24, -72, 1200, { .fields = { 40, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 16, -72, 1200, { .fields = { 40, 232 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -72, 1195, { .fields = { 40, 208 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, -64, 1200, { .fields = { 32, 120 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -64, 1200, { .fields = { 32, 112 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 40, -64, 1175, { .fields = { 48, 168 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 32, -64, 1195, { .fields = { 56, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -48, -64, 1175, { .fields = { 48, 232 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -48, -80, 1175, { .fields = { 40, 128 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -32, -96, 1181, { .fields = { 40, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -16, -96, 1187, { .fields = { 40, 216 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 16, -96, 1181, { .fields = { 32, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 0, -96, 1187, { .fields = { 32, 16 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, -80, 1175, { .fields = { 40, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 32, -80, 1195, { .fields = { 40, 112 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -16, -80, 1200, { .fields = { 64, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 0, -80, 1200, { .fields = { 24, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -64, -80, 757, { .fields = { 64, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -32, -80, 756, { .fields = { 64, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 32 } }, 0, -80, 757, { .fields = { 80, 128 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 32 } }, 32, -80, 757, { .fields = { 96, 224 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -56, -104, 750, { .fields = { 72, 216 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -32, -112, 762, { .fields = { 96, 64 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 32 } }, 0, -112, 762, { .fields = { 96, 160 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 32, -104, 751, { .fields = { 72, 192 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_acropolis_forked_road_80183F70[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 11, 0, 0, { 0, 0 } },
    { 26, 24, 0, 0, { 2, 0 } },
    { 50, 8, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_forked_road_80183FA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_forked_road_80183FB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_forked_road_80183FC0[62] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 80, 1075, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 80, 1062, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -24, 80, 1050, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 88, 1037, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 88, 1025, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, 96, 1012, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 88, 96, 1000, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 104, 987, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 72, 1037, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 80, 1025, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 1000, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 64, 1075, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 72, 1062, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 80, 1050, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 96, 1025, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 64, 96, 1037, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 80, 1062, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 64, 72, 1075, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 64, 64, 1087, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 64, 1100, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 72, 1087, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 80, 1075, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 96, 1050, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 56, 1112, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 56, 1125, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 64, 1112, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 72, 1100, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, 80, 1087, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, 96, 1062, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 64, 1125, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 72, 1112, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 64, 1137, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 80, 1100, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 72, 1125, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 80, 1112, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 40, 925, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 48, 950, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 32, 950, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 48, 937, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 48, 925, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 40, 975, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 128, 64, 975, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 64, 975, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 88, 975, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 32, 975, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 48, 975, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 64, 975, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 32, 1375, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 64, 1312, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 32, 1312, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 8, 8, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 8, -16, 1250, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -40, 1250, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, -56, 1250, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 48, 8, 1312, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 48, -16, 1312, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 48, -40, 1312, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 48, -64, 1312, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 88, -40, 1375, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 88, -16, 1375, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, 8, 1375, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -48, 1375, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_forked_road_80184498[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 27, 0, 0, { 3, 0 } },
    { 35, 5, 0, 0, { 2, 0 } },
    { 40, 7, 0, 0, { 4, 0 } },
    { 47, 15, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_forked_road_801844D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_acropolis_forked_road_801844E0[12] = {
    { { .empty = D_acropolis_forked_road_80183284 }, D_acropolis_forked_road_80183284, NULL },
    { { .elements = D_acropolis_forked_road_80183294 }, D_acropolis_forked_road_80183938, NULL },
    { { .elements = D_acropolis_forked_road_80183998 }, D_acropolis_forked_road_80183A10, NULL },
    { { .empty = D_acropolis_forked_road_80183A28 }, D_acropolis_forked_road_80183A28, NULL },
    { { .elements = D_acropolis_forked_road_80183A38 }, D_acropolis_forked_road_80183AB0, NULL },
    { { .empty = D_acropolis_forked_road_80183AC8 }, D_acropolis_forked_road_80183AC8, NULL },
    { { .elements = D_acropolis_forked_road_80183AE8 }, D_acropolis_forked_road_80183F70, NULL },
    { { .elements = D_acropolis_forked_road_80183AE8 }, D_acropolis_forked_road_80183F70, NULL },
    { { .empty = D_acropolis_forked_road_80183FA0 }, D_acropolis_forked_road_80183FA0, NULL },
    { { .empty = D_acropolis_forked_road_80183FB0 }, D_acropolis_forked_road_80183FB0, NULL },
    { { .elements = D_acropolis_forked_road_80183FC0 }, D_acropolis_forked_road_80184498, NULL },
    { { .elements = D_acropolis_forked_road_80183998 }, D_acropolis_forked_road_80183A10, NULL },
};

GpPointLight D_acropolis_forked_road_80184570[24] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5613, -698, -1760 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2621, { 0, 0 } }, 109, 4700 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8121, -599, -1760 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2621, { 0, 0 } }, 701, 3078 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6800, -720, -1259 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2785, 2703, 2621, { 0, 0 } }, 759, 3078 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1515, -1950, 1250 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 3194, 3358, { 0, 0 } }, 10, 6500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2069, -1843, 122 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2262, 2259, 2205, { 0, 0 } }, 476, 2458 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3075, -681, 256 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2621, { 0, 0 } }, 538, 2689 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3810, -80, 660 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2621, { 0, 0 } }, 100, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5010, -80, -2723 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2949, 2949, 2867, { 0, 0 } }, 200, 4200 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5010, -421, -2648 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2949, 2949, 2867, { 0, 0 } }, 602, 4200 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5010, -80, -3010 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 3031, 2949, { 0, 0 } }, 200, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -390, -3810, -2720 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2949, 2949, 2867, { 0, 0 } }, 100, 8500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -390, -3810, -2720 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2949, 2949, 2867, { 0, 0 } }, 100, 8500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -390, -3810, -2720 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2785, 2785, 2703, { 0, 0 } }, 100, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -260, -2410, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1310, 1392, 1474, { 0, 0 } }, 150, 2800 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -260, -2410, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1310, 1392, 1474, { 0, 0 } }, 150, 2800 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -260, -2410, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1310, 1392, 1474, { 0, 0 } }, 150, 2800 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -909, -1343, -2901 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2905, 2885, 2862, { 0, 0 } }, 785, 5481 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 67, -1140, -1049 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3075, 3055, 3047, { 0, 0 } }, 618, 2345 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2991, -841, -2090 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2621, 2539, 2539, { 0, 0 } }, 300, 5493 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1130, -1886, 1241 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2887, 2856, 2841, { 0, 0 } }, 894, 3098 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1040, -80, 1860 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2703, 2621, 2621, { 0, 0 } }, 100, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1040, -80, 1860 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2539, 2457, 2457, { 0, 0 } }, 100, 4200 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5690, -4590, 1250 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 3194, 3358, { 0, 0 } }, 10, 6500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3700, -3420, 1250 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3031, 3194, 3358, { 0, 0 } }, 10, 6500 },
};

GpRoomCoordSet D_acropolis_forked_road_80184E70[1] = {
    { 0, NULL, 24, D_acropolis_forked_road_80184570, 0, NULL },
};

GpViewRec D_acropolis_forked_road_80184E88[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 2130, 0x61A8, 630 } }, 541 },
    { { { { 1721, 0, 3716 }, { 310, 4081, -143 }, { -3703, 342, 1715 } }, { -849, 1600, 2900 } }, 235 },
    { { { { 1174, 0, -3924 }, { -167, 4092, -50 }, { 3920, 174, 1173 } }, { 5150, 1500, 1530 } }, 235 },
    { { { { -3906, 0, 1229 }, { 110, 4079, 349 }, { -1225, 366, -3891 } }, { 3990, 1330, -439 } }, 257 },
    { { { { 1174, 0, -3924 }, { -167, 4092, -50 }, { 3920, 174, 1173 } }, { 5150, 1500, 1530 } }, 235 },
    { { { { -662, 0, 4041 }, { 2151, 3467, 352 }, { -3421, 2180, -561 } }, { -3590, 3140, 350 } }, 257 },
    { { { { -3, 0, -4095 }, { 199, 4091, 0 }, { 4091, -199, -3 } }, { 2900, 1240, -1259 } }, 235 },
    { { { { -3, 0, -4095 }, { 199, 4091, 0 }, { 4091, -199, -3 } }, { 2900, 1240, -1259 } }, 235 },
    { { { { 587, 0, -4053 }, { -2658, 3092, -385 }, { 3060, 2686, 443 } }, { 1140, 2057, 1292 } }, 235 },
    { { { { -1274, 0, -3892 }, { -708, 4027, 232 }, { 3827, 746, -1253 } }, { 8190, 1950, 430 } }, 297 },
    { { { { -2176, 0, -3469 }, { 153, 4091, -96 }, { 3466, -181, -2174 } }, { 4930, 1460, 430 } }, 297 },
    { { { { 1174, 0, -3924 }, { -167, 4092, -50 }, { 3920, 174, 1173 } }, { 5150, 1500, 1530 } }, 235 },
};

GpScriptCmd D_acropolis_forked_road_80185038[6] = {
    { 1, 256 },
    { 514, 0 },
    { 257, 0 },
    { 4099, 0 },
    { 4, 0 },
    { 0, 0 },
};

GpScriptRec D_acropolis_forked_road_80185050[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

GpScriptCmd D_acropolis_forked_road_80185058[6] = {
    { 1, 256 },
    { 514, 0 },
    { 257, 0 },
    { 9731, 0 },
    { 4, 0 },
    { 0, 0 },
};

GpScriptRec D_acropolis_forked_road_80185070[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

s32 D_acropolis_forked_road_80185078[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_acropolis_forked_road_80185084[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_acropolis_forked_road_8018508C[1] = { 0 };

GpRoomParamRec D_acropolis_forked_road_80185094[1] = { 0 };

GpRoomParamRec D_acropolis_forked_road_8018509C[1] = {
    { 0, 0, 1, 0, D_acropolis_forked_road_80185078 },
};

GpRoomParamRec* D_acropolis_forked_road_801850A4[8] = {
    D_acropolis_forked_road_80185084,
    D_acropolis_forked_road_80185084,
    D_acropolis_forked_road_8018508C,
    D_acropolis_forked_road_80185094,
    D_acropolis_forked_road_8018509C,
    D_acropolis_forked_road_80185084,
    D_acropolis_forked_road_80185084,
    D_acropolis_forked_road_80185084,
};

/// The forked road's streamed-scene task. State 0 allocates the
/// `RoomStreamWork` block, restarts the stream frame counter, cues the stream
/// (slot-6 msg 0xFA4), captures the player's coordinate matrix and slot 3 in the
/// block and warps slot 3 to the head of the path with a 0x3E9 placement.
/// State 1 sends the same spot again as a 0x3F2. State 2 waits for slot 3 to
/// go idle (msg 0x3F0) and then queues the stream's CD read. State 3 waits for
/// the stream to come up (`CdCmd_Queue::field_1FA`), starts the script pair and
/// reparents this task under it. State 4 drives the ride, moving the camera
/// target to the `field_1EA`th path entry every frame until the pad interrupts
/// it or the path runs out at frame 0x78. State 5 stops the scene, restores
/// the save's room ids, arms the fade-out task and kills this task.
void func_acropolis_forked_road_8017DA24(Task* task)
{
    GpXformArg      place;
    GpXformArg      place2;
    u8              slot;
    RoomStreamWork* work;
    RoomStreamWork* blk;
    CdCmdQueue*     queue;

    queue = &CdCmd_Queue;
    work  = (RoomStreamWork*)task->work;
    switch (task->state) {
        case 0:
            blk        = memCalloc(0x14, 0);
            task->work = (TaskIdMap*)blk;
            if (blk == NULL) {
                taskKill(task);
                break;
            }
            queue->field_1EA = 1;
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4                    = 2;
            ((RoomStreamWork*)task->work)->mtx    = Player_Status.coordMtx;
            ((RoomStreamWork*)task->work)->target = gameGetPtrSlot(3);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            place.rot.vy = 0x400;
            place.rot.vx = 0;
            place.rot.vz = 0;
            place.pos.vx = D_acropolis_forked_road_80180F80[0].vx - 0x654;
            place.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            Gp_DispatchMsgPtr(((RoomStreamWork*)task->work)->target, 0x3E9, &place, 0);
            task->state = task->state + 1;
            break;

        case 1:
            place2.rot.vy = 0x400;
            place2.pos.vx = D_acropolis_forked_road_80180F80[0].vx;
            place2.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place2.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            Gp_DispatchMsgPtr(((RoomStreamWork*)task->work)->target, 0x3F2, &place2, 0);
            task->state = task->state + 1;
            break;

        case 2:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                slot = Stream_FindSlot((u8*)&gGameSession->at4.loc, 0, 0);
                CdCmd_Enqueue(0x61, 0, &slot);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (queue->field_1FA != 0) {
                work->script                  = Gp_SpawnScript18(D_acropolis_forked_road_80185058,
                                                                 D_acropolis_forked_road_80185070);
                gGameSession->padScriptFlags |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 4:
            work->mtx->t[0] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vx;
            work->mtx->t[1] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vy;
            work->mtx->t[2] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vz;
            if ((Pad_CheckFlag800() != 0) || ((queue->field_1EA - 1) >= 0x78)) {
                task->state = task->state + 1;
            }
            break;

        case 5:
            Gp_StateF0.field_4 = 0;
            func_800E9BDC(2, 0x9FF);
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData[0].state.at4.loc.stage = 1;
            Mc_SaveData[0].state.at4.loc.area  = 0xA;
            Mc_SaveData[0].state.at4.loc.warp  = 4;
            Mc_SaveData[0].state.at4.loc.room  = 1;
            gDisplayState.spriteVariant        = 1;
            Task_Spawn(0, 0x11, 0, 0);
            gGameSession->padScriptFlags &= 0x7F;
            taskKill(task);
            break;
    }
}

/// The forked road's return ride: the same streamed scene played backwards
/// along `D_acropolis_forked_road_80180F80`, whose entries this one walks from
/// the far end (`0x3B - CdCmd_Queue::field_1EA`).
///
/// State 0 allocates the `RoomStreamWork` block, captures slot 3 and the
/// player's coordinate matrix (`Player_Status.coordMtx`) in it, cues the stream
/// (slot-6 msg 0xFA4) and republishes the player's weapon to slot 3 with a
/// 0x3E8 record. State 1 waits for the stream to come up
/// (`CdCmd_Queue::field_1FA`), moves the player to the head of the
/// path, starts the script pair, reparents this task under it and blanks the
/// display. State 2 drives the ride: it un-blanks after two frames, walks the
/// player along the path, and lets the pad spawn the skip task. Once
/// that task reports done it warps slot 3 to the path's end with a 0x3E9 and
/// arms the ride's exit; otherwise the ride ends on its own when the path runs
/// down to its last 11 entries, which is sent as a 0x3F2. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), restores the camera
/// view and the session's ride flag and kills this task.
void func_acropolis_forked_road_8017DD60(Task* task)
{
    AnimationPlayRequest rec;
    GpXformArg           place;
    s32                  sp40;
    RoomStreamWork*      work;
    RoomStreamWork*      blk;
    CdCmdQueue*          queue;
    s32                  weaponId;

    queue = &CdCmd_Queue;
    work  = (RoomStreamWork*)task->work;
    switch (task->state) {
        case 0:
            blk        = memCalloc(0x14, 0);
            task->work = (TaskIdMap*)blk;
            if (blk == NULL) {
                taskKill(task);
                break;
            }
            ((RoomStreamWork*)task->work)->target = gameGetPtrSlot(3);
            ((RoomStreamWork*)task->work)->mtx    = Player_Status.coordMtx;
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            weaponId                 = Player_Status.weapon;
            rec.source.index         = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId          = 1;
            rec.blend                = ANIMATION_BLEND_RESET;
            rec.blendFrames          = 0;
            rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            Gp_DispatchMsgPtr(((RoomStreamWork*)task->work)->target, ANIMATION_MESSAGE_PLAY, &rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 2;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->mtx->t[0]               = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vx;
                work->mtx->t[1]               = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vy;
                work->mtx->t[2]               = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                work->script                  = Gp_SpawnScript18(D_acropolis_forked_road_80185038,
                                                                 D_acropolis_forked_road_80185050);
                gGameSession->padScriptFlags |= 0x80;
                Task_Reparent(task, work->script);
                SetDispMask(0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;

        case 2:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= 3) {
                SetDispMask(1);
            }
            work->mtx->t[0] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vx;
            work->mtx->t[1] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vy;
            work->mtx->t[2] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
            if (work->spawned != 0) {
                if (Task_PollKill(work->child, &sp40) != 0) {
                    place.pos.vx = -0x190;
                    place.pos.vy = 1;
                    place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    Gp_DispatchMsgPtr(((RoomStreamWork*)task->work)->target, 0x3E9, &place, 0);
                    Task_SpawnFromTable(D_acropolis_forked_road_80180F44, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->child   = Task_SpawnFromTable(D_acropolis_forked_road_80180F44, 3, 0, 0);
                work->spawned = 1;
            }
            if ((0x3B - queue->field_1EA) < 0xB) {
                place.pos.vx = -0x190;
                place.pos.vy = 1;
                place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                Gp_DispatchMsgPtr(((RoomStreamWork*)task->work)->target, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                Mc_SaveData[0].state.at4.loc.view = Gp_FindViewIndex(5);
                Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA5, 0, 0);
                func_800E9BDC(2, 0x9FF);
                Gp_StateF0.field_4            = 0;
                gGameSession->padScriptFlags &= 0x7F;
                taskKill(task);
            }
            break;
    }
}

/// An eight-frame screen fade: draws the fade overlay (mode 2) at the level
/// held in the task's `killCountdown`, which rises by 0x20 a frame, and asks
/// for the task to be killed once it passes 0xFF.
void func_acropolis_forked_road_8017E1C0(Task* arg0)
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

/// The same eight-frame fade run the other way: the overlay level is the
/// complement of the rising counter, so it falls from 0xFF by 0x20 a frame,
/// and the task kills itself once the counter passes 0xFF.
void func_acropolis_forked_road_8017E220(Task* arg0)
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

/// Room script callback: sets `Gp_StateF0.field_1E` to 1.
void func_acropolis_forked_road_8017E288(void)
{
    Gp_StateF0.field_1E = 1;
}

/// Forked-road ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_forked_road_80182178`, in four runs that differ only
/// in the flavour bits added to the entry's index - two 0x02000000, two
/// 0x03000000, eight 0x02000100 and two 0x00000200 - and then publishes the
/// room's three ambient sound events before marking itself done.
void func_acropolis_forked_road_8017E298(Task* task)
{
    GfxCoord* coord;
    s32       i;

    coord = task->extra.tmd->coords;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 2; i < 4; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x3000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 4; i < 0xC; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000100, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 0xC; i < 0xE; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x200, &D_acropolis_forked_road_80182178[i]);
        }
        D_80115758  = 0x60290;
        D_8011572C  = 0x60291;
        D_80115750  = 0x60292;
        task->state = task->state + 1;
    }
}

/// Draws one frame of a forked-road wall lamp: a flickering, screen-aligned
/// sprite at the task's own coordinate frame. The lamp is skipped entirely
/// while the effect pool is busy (`Gp_State1C->eventState` at 4 or more) and on
/// the days whose bit is clear in `D_acropolis_forked_road_801821E8`, indexed
/// by the low nibble of `Task::spawnArg1`.
///
/// On the first frame the task unpacks the rest of `spawnArg1` into its
/// effect work block - the half extent into `scale` (bits 16-27, 0x280 when
/// zero), the animation column into `angle` (bits 8-9) and that column's grey
/// level into `period` - and leaves only the day index behind. Every frame it then projects the
/// coordinate's translation through `GsWSMATRIX` with a single `RTPS` into a
/// 0x14-byte `G_SCRATCH_HEAD` block and, for anything at `otz` 0x11 or
/// further, queues one semi-transparent `POLY_FT4` on tpage 0x2B whose
/// half extent is `scale * 39 / otz`, so the lamp shrinks with distance. The
/// grey alternates by 0x10 on the parity of `DisplayState::field_8`, which is
/// what makes it flicker.
void func_acropolis_forked_road_8017E410(Task* task)
{
    void**            scratch;
    RoomShaftScratch* block;
    GpEffWork*        work;
    GfxCoord*         coord;
    POLY_FT4*         prim;
    s32               rgb;
    s32               flicker;
    s16               xy;

    work  = (GpEffWork*)task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState < 4 &&
        ((D_acropolis_forked_road_801821E8[task->spawnArg1.value & 0xF] >> (gGameSession->at4.loc.view - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block = (RoomShaftScratch*)*scratch;
        if (task->state == 0) {
            u8 levels[3] = { 0x50, 0x30, 0x10 };

            if (task->spawnArg1.value & 0xFFF0000) {
                work->scale = (task->spawnArg1.value >> 16) & 0xFFF;
            } else {
                work->scale = 0x280;
            }
            work->angle           = (task->spawnArg1.value >> 8) & 3;
            task->spawnArg1.value = task->spawnArg1.value & 0xF;
            work->period          = levels[work->angle];
            task->state           = task->state + 1;
        }
        block->vec.vx = (u16)coord->workm.t[0];
        block->vec.vy = (u16)coord->workm.t[1];
        block->vec.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&block->sx);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            flicker     = ((u8)gDisplayState.animFrame & 1) * 0x10;
            rgb         = (u8)work->period + flicker;
            prim->tpage = 0x2B;
            prim->r0    = rgb;
            prim->g0    = rgb;
            prim->b0    = rgb;
            setSemiTrans(prim, 1);
            setClut(prim, work->angle * 16, 0x10E);
            prim->u0 = work->angle * 0x28;
            prim->v0 = 0;
            prim->u1 = work->angle * 0x28 + 0x27;
            prim->v1 = 0;
            prim->u2 = work->angle * 0x28;
            prim->v2 = 0x27;
            prim->u3 = work->angle * 0x28 + 0x27;
            prim->v3 = 0x27;

            block->halfWidth = (work->scale * 0x27) / block->otz;
            xy               = block->sx - (u16)block->halfWidth;
            prim->x2         = xy;
            prim->x0         = xy;
            xy               = block->sx + (u16)block->halfWidth;
            prim->x3         = xy;
            prim->x1         = xy;
            xy               = block->sy - (u16)block->halfWidth;
            prim->y1         = xy;
            prim->y0         = xy;
            xy               = block->sy + (u16)block->halfWidth;
            prim->y3         = xy;
            prim->y2         = xy;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
        }
        SCRATCH_POP_BYTES(0x14);
    }
}

/// One drifting mote of the room's ambient effect. The first tick seeds it
/// from `Gp_LcgState`: a size of 0x20, a random tilt pair (`period` /
/// `step`) and a random drift in `move`. While it flies, the drift
/// moves its coordinate frame and the tilt rotates it; each drift axis eases
/// back towards zero by one a tick and re-rolls a fresh multiple of 8 when it
/// gets there, and the tilt wanders by a random step. Once the frame has
/// risen past the origin the mote fades in by 0x10 a tick up to 0x80, then
/// fades back out and releases its work block.
void func_acropolis_forked_road_8017E81C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        vy;
    s32        vx;
    s32        vz;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale   = 0x20;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state   = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            Gfx_RotMatrixX(&coord->coord, work->period, 0);
            Gfx_RotMatrixZ(&coord->coord, work->step, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;

            vy = work->move.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->move.vy = vy;

            vx = work->move.vx;
            if (vx == 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->move.vx = vx;
            }

            vz = work->move.vz;
            if (vz == 0) {
                work->move.vz += work->step % 32;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->move.vz = vz;
            }

            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step   += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_acropolis_forked_road_8017EC70(coord, work->scale, 0);
            break;
        case 2:
            if (work->angle < 0x80) {
                work->angle += 0x10;
            } else {
                task->state = 3;
            }
            func_acropolis_forked_road_8017EC70(coord, work->scale, 0);
            break;
        case 3:
            if (work->angle >= 0x11) {
                work->angle -= 0x10;
                func_acropolis_forked_road_8017EC70(coord, work->scale, work->angle);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one mote: the unit quad `D_80111E38` scaled by `arg1`, rotated and
/// placed by the mote's coordinate frame, then projected through
/// `GsWSMATRIX` into a textured quad. A mote nearer than `otz` 0x11 is not
/// drawn. `arg2` is the fade level: zero draws the texture unshaded, anything
/// else modulates it to that grey and draws it semi-transparent.
static void func_acropolis_forked_road_8017EC70(GfxCoord* coord, s32 arg1, s16 arg2)
{
    RoomQuadScratch* blk;
    POLY_FT4*        prim;
    SVECTOR*         sv;
    s32              i;

    blk = SCRATCH_PUSH(RoomQuadScratch);
    for (i = 0; i < 4; i++) {
        blk->v[i].vx = D_80111E38[i].x * arg1;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = D_80111E38[i].y * arg1;
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
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    setUV4(prim, 0, 0xE8, 7, 0xE8, 0, 0xEF, 7, 0xEF);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (arg2 != 0) {
            setRGB0(prim, arg2, arg2, arg2);
            setSemiTrans(prim, 1);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(RoomQuadScratch);
}

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_acropolis_forked_road_8017EF80(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
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
                func_acropolis_forked_road_8017F650(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_acropolis_forked_road_8017F650(coord, (s16)((u16)work->angle * 2), rgb);
                func_acropolis_forked_road_8017F224(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_acropolis_forked_road_80180554(coord, (s16)(work->angle * 3), rgb);
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

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
static void func_acropolis_forked_road_8017F224(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
static void func_acropolis_forked_road_8017F650(GfxCoord* arg0, s16 arg1, u8* rgb)
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
    SCRATCH_POP(RoomFanScratch);
}

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.
void func_acropolis_forked_road_8017F9E4(Task* task)
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
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
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
                objCoord->coord.t[0]   = D_acropolis_forked_road_80182204[0].vx;
                objCoord->coord.t[1]   = D_acropolis_forked_road_80182204[0].vy;
                objCoord->coord.t[2]   = D_acropolis_forked_road_80182204[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_acropolis_forked_road_80182204[1];
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
                    SVECTOR* edge    = &D_acropolis_forked_road_80182204[1];
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
                func_acropolis_forked_road_8017FED4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
static void func_acropolis_forked_road_8017FED4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
void func_acropolis_forked_road_801802CC(Task* task)
{
    GfxCoord*  objCoord;
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
            func_acropolis_forked_road_8017F224(objCoord, 0x100, 0x100, rgb);
            func_acropolis_forked_road_8017F224(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Queues a star-shaped glow at the projected world position of `arg0`: a
/// disc of radius `arg1` scaled by depth, shaded half `arg2` at the centre, an
/// inner disc of half that radius at full `arg2`, and four thin rays at right
/// angles, alternately reaching the radius and twice it, all fading to black
/// at the rim. Nothing is drawn when the projection overflows.
static void func_acropolis_forked_road_80180554(GfxCoord* arg0, s16 arg1, u8* arg2)
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
