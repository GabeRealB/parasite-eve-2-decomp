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

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
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
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag_ids.h"
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
#include "../../shared/room_visual_effects.h"
#include "../../shared/falling_leaves.h"

/// Set to 1 by the fade-out task once the scene has finished.

/// Per-frame path the streamed scene walks `gPlayerStatus.coordMtx` along, indexed by
/// `gCdCmdQueue::movieFrame - 1` for the 0x78 frames the ride lasts.
extern SVECTOR D_acropolis_forked_road_80180F80[];

/// The script pair the streamed scene runs.
extern PadScriptCmd              D_acropolis_forked_road_80185058[6];
extern PadScriptVibrationSegment D_acropolis_forked_road_80185070[2];

/// The script pair the return ride runs.
extern PadScriptCmd              D_acropolis_forked_road_80185038[6];
extern PadScriptVibrationSegment D_acropolis_forked_road_80185050[2];

/// The fourteen spawn offsets of the forked road's ambient effects, indexed
/// 0..13 by the first-frame burst below.
extern SVECTOR D_acropolis_forked_road_80182178[14];

/// One bit per in-game day (shifted by `GameSession::location.loc.view - 1`) for each of
/// the sixteen ambient-effect slots: which of the room's lamps are lit today.
extern u16 D_acropolis_forked_road_801821E8[14];

/// The two points the twin trail is anchored at, relative to its parent frame:
/// `[0]` places the task's own frame and `[1]`, also reached by its own name,
/// the second trail's.

void        func_acropolis_forked_road_8017DA24(Task*);
void        func_acropolis_forked_road_8017DD60(Task*);
void        func_acropolis_forked_road_8017E1C0(Task*);
static void _acropolisForkedRoadSkipFadeInTask(Task* task);

extern AnimationPlayRequest     D_acropolis_forked_road_8018207C;
extern AnimationBankCopyRequest D_acropolis_forked_road_80182060;
extern WorldCollisionGrid       D_acropolis_forked_road_80182BF0[1];
extern WorldCollisionTrigger    D_acropolis_forked_road_80182C14[6];
extern WorldCollisionTrigger    D_acropolis_forked_road_80182DDC[7];
extern WorldCoordRoomLights     D_acropolis_forked_road_80184E70[1];
static void                     _acropolisForkedRoadReleaseMaggotCaterpillarEntrance(void);

extern SpriteBatch  D_acropolis_forked_road_80183284[2];
extern SpriteBatch  D_acropolis_forked_road_80183938[12];
extern SpriteBatch  D_acropolis_forked_road_80183A10[3];
extern SpriteBatch  D_acropolis_forked_road_80183A28[2];
extern SpriteBatch  D_acropolis_forked_road_80183AB0[3];
extern SpriteBatch  D_acropolis_forked_road_80183AC8[2];
extern SpriteBatch  D_acropolis_forked_road_80183F70[6];
extern SpriteBatch  D_acropolis_forked_road_80183FA0[2];
extern SpriteBatch  D_acropolis_forked_road_80183FB0[2];
extern SpriteBatch  D_acropolis_forked_road_80184498[7];
extern SpriteSource D_acropolis_forked_road_80183294[85];
extern SpriteSource D_acropolis_forked_road_80183998[6];
extern SpriteSource D_acropolis_forked_road_80183A38[6];
extern SpriteSource D_acropolis_forked_road_80183AE8[58];
extern SpriteSource D_acropolis_forked_road_80183FC0[62];

TaskDesc D_acropolis_forked_road_80180F44[5] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_forked_road_8017DA24, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_forked_road_8017DD60, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_forked_road_8017E1C0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisForkedRoadSkipFadeInTask, { .value = 0 } },
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

static AnimationPackedPose _gAcropolisForkedRoadAnimation045FCBank1[6] = {
#include "assets/acropolis_forked_road_animation_045FC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisForkedRoadAnimation045FCBank4[46] = {
#include "assets/acropolis_forked_road_animation_045FC_bank4.inc"
};

static AnimationRecord _gAcropolisForkedRoadAnimation045FCRecords[109] = {
#include "assets/acropolis_forked_road_animation_045FC_records.inc"
};

static u16 _gAcropolisForkedRoadAnimation045FCIndices[20] = {
#include "assets/acropolis_forked_road_animation_045FC_indices.inc"
};

static AnimationSet _gAcropolisForkedRoadAnimation045FC = {
    _gAcropolisForkedRoadAnimation045FCRecords,
    _gAcropolisForkedRoadAnimation045FCIndices,
    { NULL, _gAcropolisForkedRoadAnimation045FCBank1, NULL, NULL, _gAcropolisForkedRoadAnimation045FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisForkedRoadAnimation04A6CBank1[10] = {
#include "assets/acropolis_forked_road_animation_04A6C_bank1.inc"
};

static AnimationPackedRotation _gAcropolisForkedRoadAnimation04A6CBank4[95] = {
#include "assets/acropolis_forked_road_animation_04A6C_bank4.inc"
};

static AnimationRecord _gAcropolisForkedRoadAnimation04A6CRecords[139] = {
#include "assets/acropolis_forked_road_animation_04A6C_records.inc"
};

static u16 _gAcropolisForkedRoadAnimation04A6CIndices[20] = {
#include "assets/acropolis_forked_road_animation_04A6C_indices.inc"

};

static AnimationSet _gAcropolisForkedRoadAnimation04A6C = {
    _gAcropolisForkedRoadAnimation04A6CRecords,
    _gAcropolisForkedRoadAnimation04A6CIndices,
    { NULL, _gAcropolisForkedRoadAnimation04A6CBank1, NULL, NULL, _gAcropolisForkedRoadAnimation04A6CBank4, NULL, NULL, NULL },
};

AnimationSet* D_acropolis_forked_road_80182054[3] = {
    NULL,
    &_gAcropolisForkedRoadAnimation045FC,
    &_gAcropolisForkedRoadAnimation04A6C,
};

AnimationBankCopyRequest D_acropolis_forked_road_80182060 = { { .sets = D_acropolis_forked_road_80182054 }, ARRAY_SIZE(D_acropolis_forked_road_80182054) };

AnimationPlayRequest D_acropolis_forked_road_80182068 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_forked_road_8018207C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_forked_road_80182090[2] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
};

EvsCommand D_acropolis_forked_road_801820B8[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_forked_road_80182060 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_forked_road_8018207C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisForkedRoadReleaseMaggotCaterpillarEntrance }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
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

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_acropolis_forked_road_80182214[3] = {
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
    gViewIdentityMap,
    D_acropolis_forked_road_80182244,
    D_acropolis_forked_road_80182250,
};

ViewCount D_acropolis_forked_road_80182268[3] = { 10, 9, 9 };

WorldCoordRoomLighting D_acropolis_forked_road_80182270[3] = {
    { D_acropolis_forked_road_80184E70, NULL },
    { D_acropolis_forked_road_80184E70, NULL },
    { D_acropolis_forked_road_80184E70, NULL },
};

DirectionWarpEntry D_acropolis_forked_road_80182288[5] = {
    { { { .word = 1920 }, -3791, 1, 833 }, { 0, 0, 0, 0 }, { { .word = 1920 }, -3791, 1, 833 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SECURITY_ROOM },
    { { { .word = 0 }, -5107, 1, -4030 }, { 0, 0, 0, 0 }, { { .word = 0 }, -5107, 1, -4030 }, { 0, 0, 0, 0 }, 0x51090003, 0x51090002, 0x51090006, 4, DIRECTION_WARP_FLAG_NONE, 495 },
    { { { .word = 1792 }, -4094, -470, 1325 }, { 0, 0, 0, 0 }, { { .word = 1792 }, -3764, -600, 797 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -261, 1, 1205 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SECURITY_ROOM },
};

static SVECTOR _gAcropolisForkedRoadCollision05630Normals[18] = {
#include "assets/acropolis_forked_road_collision_05630_normals.inc"
};

static SVECTOR _gAcropolisForkedRoadCollision05630Verts[118] = {
#include "assets/acropolis_forked_road_collision_05630_verts.inc"
};

static WorldCollisionGridFace _gAcropolisForkedRoadCollision05630Faces[38] = {
#include "assets/acropolis_forked_road_collision_05630_faces.inc"
};

static s16 _gAcropolisForkedRoadCollision05630Cells[244] = {
#include "assets/acropolis_forked_road_collision_05630_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisForkedRoadCollision05630Cells[i])
static s16* _gAcropolisForkedRoadCollision05630Table[24] = {
#include "assets/acropolis_forked_road_collision_05630_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_forked_road_80182BF0[1] = {
    { NULL, _gAcropolisForkedRoadCollision05630Normals, _gAcropolisForkedRoadCollision05630Verts, _gAcropolisForkedRoadCollision05630Faces, _gAcropolisForkedRoadCollision05630Table, 0x303E, 6900, 6, 4, 4000, 38 },
};

WorldCollisionTrigger D_acropolis_forked_road_80182C14[6] = {
    { NULL, NULL, NULL, { -1905, 0, -353, 0 }, { { -509, 3168, 2239, 0 }, { 510, 3168, -2238, 0 }, { -509, -3168, 2239, 0 }, { 510, -3168, -2238, 0 } }, { 3996, 0, 909, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1712, 0, -149, 0 }, { { 526, 3168, -2286, 0 }, { -525, 3168, 2287, 0 }, { 526, -3168, -2286, 0 }, { -525, -3168, 2287, 0 } }, { -4002, 0, -920, 0 }, { 0, 0, 4096, 0 }, 3941, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6711, 0, -2121, 0 }, { { 1938, 3168, 482, 0 }, { -1937, 3168, -481, 0 }, { 1938, -3168, 482, 0 }, { -1937, -3168, -481, 0 } }, { 989, 0, -3983, 0 }, { 0, 0, 4096, 0 }, 3736, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6668, 0, -2514, 0 }, { { -1920, 3168, -512, 0 }, { 1920, 3168, 513, 0 }, { -1920, -3168, -512, 0 }, { 1920, -3168, 513, 0 } }, { -1058, 0, 3959, 0 }, { 0, 0, 4096, 0 }, 3736, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3905, 0, -3010, 0 }, { { -943, 3168, 1067, 0 }, { 943, 3168, -1066, 0 }, { -943, -3168, 1067, 0 }, { 943, -3168, -1066, 0 } }, { 3088, 0, 2731, 0 }, { 0, 0, 4096, 0 }, 3472, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3575, 0, -2822, 0 }, { { 1290, 3168, -1366, 0 }, { -1290, 3168, 1367, 0 }, { 1290, -3168, -1366, 0 }, { -1290, -3168, 1367, 0 } }, { -2990, 0, -2822, 0 }, { 0, 0, 4096, 0 }, 3683, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_forked_road_80182DDC[7] = {
    { NULL, NULL, NULL, { -5024, -64, -4192, 0 }, { { -896, 0, -768, 0 }, { 896, 0, -768, 0 }, { -896, 0, 768, 0 }, { 896, 0, 768, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_WARP, 8, 50, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -192, -96, 1440, 0 }, { { -768, 0, -736, 0 }, { 768, 0, -736, 0 }, { -768, 0, 736, 0 }, { 768, 0, 736, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 36, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3905, -128, 1039, 0 }, { { -433, 0, -633, 0 }, { 721, 0, -218, 0 }, { -720, 0, 219, 0 }, { 434, 0, 634, 0 } }, { 0, 4114, 0, 0 }, { 1189, 0, -3920, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_FACING, 68, 244, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1049, -96, -712, 0 }, { { -1285, 0, 644, 0 }, { -1287, 0, -1365, 0 }, { 232, 0, 613, 0 }, { 231, 0, -468, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1872, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3922, -656, 1150, 0 }, { { -1191, -1520, -451, 0 }, { 1192, -1520, 452, 0 }, { -1191, 1520, -451, 0 }, { 1192, 1520, 452, 0 } }, { 1451, 0, -3834, 0 }, { 1380, 0, -3857, 0 }, 1982, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 22, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4113, -512, 1439, 0 }, { { -983, 0, -519, 0 }, { 1087, 0, 270, 0 }, { -1087, 0, -270, 0 }, { 983, 0, 519, 0 } }, { 0, 4098, 0, 0 }, { -1568, 0, 3784, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100 | WORLD_COLLISION_TRIGGER_AUTOMATIC, 66, 112, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2336, -96, -512, 0 }, { { -256, 0, -2624, 0 }, { 1344, 0, -1600, 0 }, { -1344, 0, 1600, 0 }, { 256, 0, 2624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 2635, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_forked_road_80182FF0[2] = {
    { 19, 19, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_301900_80179120 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183008[3] = {
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401000_80155004 },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_801693AC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_8018302C[2] = {
    { 55, 55, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105500_8013A8DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183044[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_200700_80150C80 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183068[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183080[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183098[3] = {
    { 7, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor00700_D06E60 },
    { 8, 7, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor00700_D075A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_801830BC[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_801830D4[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_801830EC[2] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183104[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_8018311C[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183134[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_8018314C[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183164[3] = {
    { 46, 46, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04600_D05878 },
    { 47, 47, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor04600_D0649C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_forked_road_80183188[5] = {
    { 70, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_8013F5F0 },
    { 71, 71, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_80139E60 },
    { 72, 72, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_80153EC8 },
    { 73, 73, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_207200_8014E7A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_forked_road_801831C4[24] = {
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

SpriteBatch D_acropolis_forked_road_80183284[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_forked_road_80183294[85] = {
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

SpriteBatch D_acropolis_forked_road_80183938[12] = {
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
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_forked_road_80183998[6] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 937, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 937, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 937, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -88, 937, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -24, 937, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_forked_road_80183A10[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_80183A28[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_forked_road_80183A38[6] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 24, 937, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -16, 937, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 937, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -88, 937, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -24, 937, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_forked_road_80183AB0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_80183AC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_80183AD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_forked_road_80183AE8[58] = {
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
    { 143, 0x4000, { .fields = { 8, 8 } }, -40, -88, 1178, { .fields = { 40, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 32, -88, 1178, { .fields = { 40, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, -80, 1185, { .fields = { 40, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, -24, -80, 1197, { .fields = { 64, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 16, -80, 1197, { .fields = { 48, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -80, 1195, { .fields = { 40, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, -72, 1195, { .fields = { 40, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, -24, -72, 1200, { .fields = { 40, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 16, -72, 1200, { .fields = { 40, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -72, 1195, { .fields = { 40, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, -64, 1200, { .fields = { 32, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -64, 1200, { .fields = { 32, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, 40, -64, 1175, { .fields = { 48, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 32, -64, 1195, { .fields = { 56, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -48, -64, 1175, { .fields = { 48, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -48, -80, 1175, { .fields = { 40, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -32, -96, 1181, { .fields = { 40, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -16, -96, 1187, { .fields = { 40, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, 16, -96, 1181, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, 0, -96, 1187, { .fields = { 32, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, -80, 1175, { .fields = { 40, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 32, -80, 1195, { .fields = { 40, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -16, -80, 1200, { .fields = { 64, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, 0, -80, 1200, { .fields = { 24, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, -64, -80, 757, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, -32, -80, 756, { .fields = { 64, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, 0, -80, 757, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, 32, -80, 757, { .fields = { 96, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -56, -104, 750, { .fields = { 72, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, -32, -112, 762, { .fields = { 96, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 32 } }, 0, -112, 762, { .fields = { 96, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, 32, -104, 751, { .fields = { 72, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_forked_road_80183F70[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 11, 0, 0, { 0, 0 } },
    { 26, 24, 0, 0, { 2, 0 } },
    { 50, 8, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_80183FA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_80183FB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_forked_road_80183FC0[62] = {
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

SpriteBatch D_acropolis_forked_road_80184498[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 27, 0, 0, { 3, 0 } },
    { 35, 5, 0, 0, { 2, 0 } },
    { 40, 7, 0, 0, { 4, 0 } },
    { 47, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_forked_road_801844D0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_forked_road_801844E0[12] = {
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

/// Point lights for model shading in every view of the forked road.
///
/// Shared by the three room entries. Positions and inner/outer falloff radii
/// use integer world units; RGB intensities have 12 fractional bits (`ONE` is 1.0).
/// Runtime updates the transform parents, composition caches and attenuation.
/// The room-light collection borrows this array while the overlay is loaded.
static WorldCoordPointLight _gAcropolisForkedRoadPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5613, -698, -1760 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 109, 4700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8121, -599, -1760 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 701, 3078 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6800, -720, -1259 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2703, 2621 }, { 0, 0 } }, 759, 3078 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1515, -1950, 1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3194, 3358 }, { 0, 0 } }, 10, 6500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2069, -1843, 122 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2262, 2259, 2205 }, { 0, 0 } }, 476, 2458 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3075, -681, 256 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 538, 2689 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3810, -80, 660 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 100, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5010, -80, -2723 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 200, 4200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5010, -421, -2648 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 602, 4200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5010, -80, -3010 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3031, 2949 }, { 0, 0 } }, 200, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -390, -3810, -2720 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 100, 8500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -390, -3810, -2720 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2949, 2949, 2867 }, { 0, 0 } }, 100, 8500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -390, -3810, -2720 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2785, 2703 }, { 0, 0 } }, 100, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -260, -2410, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1392, 1474 }, { 0, 0 } }, 150, 2800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -260, -2410, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1392, 1474 }, { 0, 0 } }, 150, 2800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -260, -2410, 1040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1392, 1474 }, { 0, 0 } }, 150, 2800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -909, -1343, -2901 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2905, 2885, 2862 }, { 0, 0 } }, 785, 5481 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 67, -1140, -1049 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3075, 3055, 3047 }, { 0, 0 } }, 618, 2345 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2991, -841, -2090 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2621, 2539, 2539 }, { 0, 0 } }, 300, 5493 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1130, -1886, 1241 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2887, 2856, 2841 }, { 0, 0 } }, 894, 3098 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1040, -80, 1860 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2703, 2621, 2621 }, { 0, 0 } }, 100, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1040, -80, 1860 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2539, 2457, 2457 }, { 0, 0 } }, 100, 4200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5690, -4590, 1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3194, 3358 }, { 0, 0 } }, 10, 6500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3700, -3420, 1250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 3194, 3358 }, { 0, 0 } }, 10, 6500 },
};

WorldCoordRoomLights D_acropolis_forked_road_80184E70[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisForkedRoadPointLights), _gAcropolisForkedRoadPointLights, 0, NULL },
};

ViewCamera D_acropolis_forked_road_80184E88[12] = {
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

PadScriptCmd D_acropolis_forked_road_80185038[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 16), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_forked_road_80185050[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

PadScriptCmd D_acropolis_forked_road_80185058[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 38), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_forked_road_80185070[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

WorldCollisionFootstepSounds D_acropolis_forked_road_80185078 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_forked_road_80185084[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_forked_road_8018508C[1] = { 0 };

WorldCollisionSurfaceProperties D_acropolis_forked_road_80185094[1] = { 0 };

WorldCollisionSurfaceProperties D_acropolis_forked_road_8018509C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_forked_road_80185078 },
};

WorldCollisionSurfaceProperties* D_acropolis_forked_road_801850A4[8] = {
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
/// `RoomMoviePathWork` block, restarts the stream frame counter, cues the stream
/// (slot-6 msg 0xFA4), captures the player's coordinate matrix and slot 3 in the
/// block and warps slot 3 to the head of the path with a 0x3E9 placement.
/// State 1 sends the same spot again as a 0x3F2. State 2 waits for slot 3 to
/// go idle (msg 0x3F0) and then queues the stream's CD read. State 3 waits for
/// the stream to come up (`gCdCmdQueue::movieReady`), starts the script pair and
/// adopts its task as a child. State 4 drives the ride, moving the camera
/// target to the `field_1EA`th path entry every frame until the pad interrupts
/// it or the path runs out at frame 0x78. State 5 stops the scene, restores
/// the save's room ids, arms the fade-out task and kills this task.
void func_acropolis_forked_road_8017DA24(Task* task)
{
    ActorTransform     place;
    ActorTransform     place2;
    u8                 slot;
    RoomMoviePathWork* work;
    CdCmdQueue*        queue;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case 0:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            queue->movieFrame = 1;
            func_800E9BDC(3, 0x9FF);
            gSceneCombatState.actorControl               = SCENE_COMBAT_ACTORS_HIDDEN;
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            place.rot.vy = 0x400;
            place.rot.vx = 0;
            place.rot.vz = 0;
            place.pos.vx = D_acropolis_forked_road_80180F80[0].vx - 0x654;
            place.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
            task->state = task->state + 1;
            break;

        case 1:
            place2.rot.vy = 0x400;
            place2.pos.vx = D_acropolis_forked_road_80180F80[0].vx;
            place2.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place2.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place2, 0);
            task->state = task->state + 1;
            break;

        case 2:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                slot = streamFindMovieSlot(&gGameSession->location.loc, 0, 0);
                cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, &slot);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (queue->movieReady != 0) {
                work->padScriptTask           = Gp_SpawnScript18(D_acropolis_forked_road_80185058,
                                                                 D_acropolis_forked_road_80185070);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case 4:
            work->playerMtx->t[0] = D_acropolis_forked_road_80180F80[queue->movieFrame - 1].vx;
            work->playerMtx->t[1] = D_acropolis_forked_road_80180F80[queue->movieFrame - 1].vy;
            work->playerMtx->t[2] = D_acropolis_forked_road_80180F80[queue->movieFrame - 1].vz;
            if ((Pad_CheckFlag800() != 0) || ((queue->movieFrame - 1) >= 0x78)) {
                task->state = task->state + 1;
            }
            break;

        case 5:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            func_800E9BDC(2, 0x9FF);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_OBSERVATORY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            gDisplayState.spriteVariant                                 = 1;
            Task_Spawn(0, 0x11, 0, 0);
            gGameSession->padScriptFlags &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// The forked road's return ride: the same streamed scene played backwards
/// along `D_acropolis_forked_road_80180F80`, whose entries this one walks from
/// the far end (`0x3B - gCdCmdQueue::movieFrame`).
///
/// State 0 allocates the `RoomMoviePathWork` block, captures slot 3 and the
/// player's coordinate matrix (`gPlayerStatus.coordMtx`) in it, cues the stream
/// (slot-6 msg 0xFA4) and republishes the player's weapon to slot 3 with a
/// 0x3E8 record. State 1 waits for the stream to come up
/// (`gCdCmdQueue::movieReady`), moves the player to the head of the
/// path, starts the script pair, adopts its task as a child and blanks the
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
    ActorTransform       place;
    s32                  sp40;
    RoomMoviePathWork*   work;
    CdCmdQueue*          queue;
    s32                  weaponId;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case 0:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            weaponId                 = gPlayerStatus.weapon;
            rec.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId          = 1;
            rec.blend                = ANIMATION_BLEND_RESET;
            rec.blendFrames          = 0;
            rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            func_800E9BDC(3, 0x9FF);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->playerMtx->t[0]         = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vx;
                work->playerMtx->t[1]         = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vy;
                work->playerMtx->t[2]         = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vz;
                work->padScriptTask           = Gp_SpawnScript18(D_acropolis_forked_road_80185038,
                                                                 D_acropolis_forked_road_80185050);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
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
            work->playerMtx->t[0] = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vx;
            work->playerMtx->t[1] = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vy;
            work->playerMtx->t[2] = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vz;
            if (work->skipFadeStarted != 0) {
                if (Task_PollKill(work->skipFadeTask, &sp40) != 0) {
                    place.pos.vx = -0x190;
                    place.pos.vy = 1;
                    place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vz;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
                    taskSpawnFromTable(D_acropolis_forked_road_80180F44, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_forked_road_80180F44, 3, 0, 0);
                work->skipFadeStarted = 1;
            }
            if ((0x3B - queue->movieFrame) < 0xB) {
                place.pos.vx = -0x190;
                place.pos.vy = 1;
                place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->movieFrame].vz;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = Gp_FindViewIndex(5);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                func_800E9BDC(2, 0x9FF);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
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
    fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        Task_RequestKill(arg0, 0);
    }
}

/// Reveals the return ride's final pose after its skip fade has hidden the scene.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws eight
/// subtractive overlays, from brightness 255 down to 31 in steps of 32, then
/// kills the task. The signed 16-bit counter is callback-owned fade progress.
static void _acropolisForkedRoadSkipFadeInTask(Task* task)
{
    enum { SKIP_FADE_LEVEL_STEP = 32,
           SKIP_FADE_LEVEL_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = ~(u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + SKIP_FADE_LEVEL_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= SKIP_FADE_LEVEL_SPAN) {
        taskKill(task);
    }
}

/// Releases the room's scripted Maggot/Caterpillar entrances.
static void _acropolisForkedRoadReleaseMaggotCaterpillarEntrance(void)
{
    gSceneCombatState.maggotCaterpillarEntranceReady = true;
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

    coord = task->extra.coordBody->coord;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, coord, i + 0x2000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 2; i < 4; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, coord, i + 0x3000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 4; i < 0xC; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, coord, i + 0x2000100, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 0xC; i < 0xE; i++) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, coord, i + 0x200, &D_acropolis_forked_road_80182178[i]);
        }
        gRoomEffectFlashId      = EFFECT_ACROPOLIS_FORKED_ROAD_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_ACROPOLIS_FORKED_ROAD_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_ACROPOLIS_FORKED_ROAD_SPARK_BURST;
        task->state             = task->state + 1;
    }
}

/// Positions the wall-lamp glow quad as a screen-aligned square.
///
/// Requires an initialized `projection->screenPos` centre and nonnegative
/// `projection->halfExtent` half-width/half-height in pixels. Their sums and
/// differences must fit signed 32 bits. Corner indices 0..3 are top-left,
/// top-right, bottom-left and bottom-right. Each edge narrows to signed 16 bits
/// without clipping. Borrows both objects for this call and writes only X/Y.
static inline void _acropolisForkedRoadSetWallLampBounds(POLY_FT4* quad, const RoomGlowSpriteScratch* projection)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left     = projection->screenPos.vx - projection->halfExtent;
    quad->x2 = left;
    quad->x0 = left;
    right    = projection->screenPos.vx + projection->halfExtent;
    quad->x3 = right;
    quad->x1 = right;
    top      = projection->screenPos.vy - projection->halfExtent;
    quad->y1 = top;
    quad->y0 = top;
    bottom   = projection->screenPos.vy + projection->halfExtent;
    quad->y3 = bottom;
    quad->y2 = bottom;
}

void acropolisForkedRoadWallLampTask(Task* task)
{
    enum {
        WALL_LAMP_INITIALIZE     = 0,
        WALL_LAMP_INDEX_MASK     = 0xF,
        WALL_LAMP_CELL_SHIFT     = 8,
        WALL_LAMP_CELL_MASK      = 3,
        WALL_LAMP_CELL_COUNT     = 3,
        WALL_LAMP_SCALE_SHIFT    = 16,
        WALL_LAMP_SCALE_MASK     = 0xFFF,
        WALL_LAMP_DEFAULT_SCALE  = 640,
        WALL_LAMP_MIN_DRAW_DEPTH = 17,
        WALL_LAMP_FLICKER_STEP   = 16,
        WALL_LAMP_TEXTURE_PAGE   = getTPage(0, GPU_BLEND_ADD, 704, 0),
        WALL_LAMP_CLUT_X_STRIDE  = 16,
        WALL_LAMP_CLUT_Y         = 270,
        WALL_LAMP_CELL_WIDTH     = 40,
        WALL_LAMP_LAST_TEXEL     = WALL_LAMP_CELL_WIDTH - 1,
    };
    RoomGlowSpriteScratch* projection;
    EffectWork*            work;
    GfxCoord*              coord;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((D_acropolis_forked_road_801821E8[task->spawnArg1.value & WALL_LAMP_INDEX_MASK] >> (gGameSession->location.loc.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == WALL_LAMP_INITIALIZE) {
            u8 restingLevels[WALL_LAMP_CELL_COUNT] = { 0x50, 0x30, 0x10 };

            // Decode once, retaining only the lamp's view-mask index in the spawn argument.
            if (task->spawnArg1.value & (WALL_LAMP_SCALE_MASK << WALL_LAMP_SCALE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> WALL_LAMP_SCALE_SHIFT) & WALL_LAMP_SCALE_MASK;
            } else {
                work->scale = WALL_LAMP_DEFAULT_SCALE;
            }
            work->angle           = (task->spawnArg1.value >> WALL_LAMP_CELL_SHIFT) & WALL_LAMP_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & WALL_LAMP_INDEX_MASK;
            work->period          = restingLevels[work->angle];
            task->state++;
        }
        // Rejected projections still consume one frame-arena packet.
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        if (projection->otz >= WALL_LAMP_MIN_DRAW_DEPTH) {
            flickerLevel = ((u8)gDisplayState.animFrame & 1) * WALL_LAMP_FLICKER_STEP;
            greyLevel    = (u8)work->period + flickerLevel;
            quad->tpage  = WALL_LAMP_TEXTURE_PAGE;
            quad->r0     = greyLevel;
            quad->g0     = greyLevel;
            quad->b0     = greyLevel;
            setSemiTrans(quad, true);
            setClut(quad, work->angle * WALL_LAMP_CLUT_X_STRIDE, WALL_LAMP_CLUT_Y);
            quad->u0 = work->angle * WALL_LAMP_CELL_WIDTH;
            quad->v0 = 0;
            quad->u1 = work->angle * WALL_LAMP_CELL_WIDTH + WALL_LAMP_LAST_TEXEL;
            quad->v1 = 0;
            quad->u2 = work->angle * WALL_LAMP_CELL_WIDTH;
            quad->v2 = WALL_LAMP_LAST_TEXEL;
            quad->u3 = work->angle * WALL_LAMP_CELL_WIDTH + WALL_LAMP_LAST_TEXEL;
            quad->v3 = WALL_LAMP_LAST_TEXEL;

            projection->halfExtent = (work->scale * WALL_LAMP_LAST_TEXEL) / projection->otz;
            _acropolisForkedRoadSetWallLampBounds(quad, projection);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

#include "../../shared/falling_leaves_task.inc.c"

void acropolisForkedRoadLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void acropolisForkedRoadRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void acropolisForkedRoadRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_acropolis_forked_road_801802CC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
