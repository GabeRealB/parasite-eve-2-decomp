#include "acropolis_observatory_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_combat.h"

#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"

/// Per-frame paths the two streamed scenes walk the player's matrix along,
/// indexed by `gCdCmdQueue.movieFrame + 0xA8`, each with the script pair its
/// scene runs.
extern SVECTOR D_acropolis_observatory_8017E80C[];

extern SVECTOR D_acropolis_observatory_8017F16C[];

void func_acropolis_observatory_8017D9A8(Task*);
void func_acropolis_observatory_8017DD3C(Task*);
void func_acropolis_observatory_8017E0D4(Task*);
void func_acropolis_observatory_8017E134(Task*);

TaskDesc D_acropolis_observatory_8017E7DC[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017D9A8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017DD3C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017E0D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017E134, { .value = 0 } },
};

SVECTOR D_acropolis_observatory_8017E80C[300] = {
    { -8465, 5, -1750, 0 },
    { -8439, 5, -1750, 0 },
    { -8414, 5, -1750, 0 },
    { -8388, 5, -1750, 0 },
    { -8363, 5, -1750, 0 },
    { -8337, 5, -1750, 0 },
    { -8312, 5, -1750, 0 },
    { -8287, 5, -1750, 0 },
    { -8261, 5, -1750, 0 },
    { -8236, 5, -1750, 0 },
    { -8210, 5, -1750, 0 },
    { -8185, 5, -1750, 0 },
    { -8160, 2, -1750, 0 },
    { -8139, -11, -1750, 0 },
    { -8118, -26, -1750, 0 },
    { -8097, -40, -1750, 0 },
    { -8076, -54, -1750, 0 },
    { -8054, -68, -1750, 0 },
    { -8033, -82, -1750, 0 },
    { -8012, -96, -1750, 0 },
    { -7991, -110, -1750, 0 },
    { -7970, -124, -1750, 0 },
    { -7949, -138, -1750, 0 },
    { -7928, -152, -1750, 0 },
    { -7906, -167, -1750, 0 },
    { -7885, -181, -1750, 0 },
    { -7864, -195, -1750, 0 },
    { -7843, -209, -1750, 0 },
    { -7822, -223, -1750, 0 },
    { -7801, -237, -1750, 0 },
    { -7779, -251, -1750, 0 },
    { -7758, -265, -1750, 0 },
    { -7737, -279, -1750, 0 },
    { -7716, -294, -1750, 0 },
    { -7695, -308, -1750, 0 },
    { -7674, -322, -1750, 0 },
    { -7652, -336, -1750, 0 },
    { -7631, -350, -1750, 0 },
    { -7610, -364, -1750, 0 },
    { -7589, -378, -1750, 0 },
    { -7568, -392, -1750, 0 },
    { -7547, -406, -1750, 0 },
    { -7526, -420, -1750, 0 },
    { -7504, -435, -1750, 0 },
    { -7483, -449, -1750, 0 },
    { -7462, -463, -1750, 0 },
    { -7441, -477, -1750, 0 },
    { -7420, -491, -1750, 0 },
    { -7399, -505, -1750, 0 },
    { -7377, -519, -1750, 0 },
    { -7356, -533, -1750, 0 },
    { -7335, -547, -1750, 0 },
    { -7314, -562, -1750, 0 },
    { -7293, -576, -1750, 0 },
    { -7272, -590, -1750, 0 },
    { -7251, -604, -1750, 0 },
    { -7229, -618, -1750, 0 },
    { -7208, -632, -1750, 0 },
    { -7187, -646, -1750, 0 },
    { -7166, -660, -1750, 0 },
    { -7145, -674, -1750, 0 },
    { -7124, -688, -1750, 0 },
    { -7102, -703, -1750, 0 },
    { -7081, -717, -1750, 0 },
    { -7060, -731, -1750, 0 },
    { -7039, -745, -1750, 0 },
    { -7018, -759, -1750, 0 },
    { -6997, -773, -1750, 0 },
    { -6975, -787, -1750, 0 },
    { -6954, -801, -1750, 0 },
    { -6933, -815, -1750, 0 },
    { -6912, -829, -1750, 0 },
    { -6891, -844, -1750, 0 },
    { -6870, -858, -1750, 0 },
    { -6849, -872, -1750, 0 },
    { -6827, -886, -1750, 0 },
    { -6806, -900, -1750, 0 },
    { -6785, -914, -1750, 0 },
    { -6764, -928, -1750, 0 },
    { -6743, -942, -1750, 0 },
    { -6722, -956, -1750, 0 },
    { -6700, -971, -1750, 0 },
    { -6679, -985, -1750, 0 },
    { -6658, -999, -1750, 0 },
    { -6637, -1013, -1750, 0 },
    { -6616, -1027, -1750, 0 },
    { -6595, -1041, -1750, 0 },
    { -6574, -1055, -1750, 0 },
    { -6552, -1069, -1750, 0 },
    { -6531, -1083, -1750, 0 },
    { -6510, -1097, -1750, 0 },
    { -6489, -1112, -1750, 0 },
    { -6468, -1126, -1750, 0 },
    { -6447, -1140, -1750, 0 },
    { -6425, -1154, -1750, 0 },
    { -6404, -1168, -1750, 0 },
    { -6383, -1182, -1750, 0 },
    { -6362, -1196, -1750, 0 },
    { -6341, -1210, -1750, 0 },
    { -6320, -1224, -1750, 0 },
    { -6299, -1238, -1750, 0 },
    { -6277, -1253, -1750, 0 },
    { -6256, -1267, -1750, 0 },
    { -6235, -1281, -1750, 0 },
    { -6214, -1295, -1750, 0 },
    { -6193, -1309, -1750, 0 },
    { -6172, -1323, -1750, 0 },
    { -6150, -1337, -1750, 0 },
    { -6129, -1351, -1750, 0 },
    { -6108, -1365, -1750, 0 },
    { -6087, -1380, -1750, 0 },
    { -6066, -1394, -1750, 0 },
    { -6045, -1408, -1750, 0 },
    { -6023, -1422, -1750, 0 },
    { -6002, -1436, -1750, 0 },
    { -5981, -1450, -1750, 0 },
    { -5960, -1464, -1750, 0 },
    { -5939, -1478, -1750, 0 },
    { -5918, -1492, -1750, 0 },
    { -5897, -1506, -1750, 0 },
    { -5875, -1521, -1750, 0 },
    { -5854, -1535, -1750, 0 },
    { -5833, -1549, -1750, 0 },
    { -5812, -1563, -1750, 0 },
    { -5791, -1577, -1750, 0 },
    { -5770, -1591, -1750, 0 },
    { -5748, -1605, -1750, 0 },
    { -5727, -1619, -1750, 0 },
    { -5706, -1633, -1750, 0 },
    { -5685, -1648, -1750, 0 },
    { -5664, -1662, -1750, 0 },
    { -5643, -1676, -1750, 0 },
    { -5622, -1690, -1750, 0 },
    { -5600, -1704, -1750, 0 },
    { -5579, -1718, -1750, 0 },
    { -5558, -1732, -1750, 0 },
    { -5537, -1746, -1750, 0 },
    { -5516, -1760, -1750, 0 },
    { -5495, -1774, -1750, 0 },
    { -5473, -1789, -1750, 0 },
    { -5452, -1803, -1750, 0 },
    { -5431, -1817, -1750, 0 },
    { -5410, -1831, -1750, 0 },
    { -5389, -1845, -1750, 0 },
    { -5368, -1859, -1750, 0 },
    { -5346, -1873, -1750, 0 },
    { -5325, -1887, -1750, 0 },
    { -5304, -1901, -1750, 0 },
    { -5283, -1915, -1750, 0 },
    { -5262, -1930, -1750, 0 },
    { -5241, -1944, -1750, 0 },
    { -5220, -1958, -1750, 0 },
    { -5198, -1972, -1750, 0 },
    { -5177, -1986, -1750, 0 },
    { -5156, -2000, -1750, 0 },
    { -5135, -2014, -1750, 0 },
    { -5114, -2028, -1750, 0 },
    { -5093, -2042, -1750, 0 },
    { -5071, -2057, -1750, 0 },
    { -5050, -2071, -1750, 0 },
    { -5029, -2085, -1750, 0 },
    { -5008, -2099, -1750, 0 },
    { -4987, -2113, -1750, 0 },
    { -4966, -2127, -1750, 0 },
    { -4945, -2141, -1750, 0 },
    { -4923, -2155, -1750, 0 },
    { -4902, -2169, -1750, 0 },
    { -4881, -2183, -1750, 0 },
    { -4860, -2198, -1750, 0 },
    { -4839, -2212, -1750, 0 },
    { -4818, -2226, -1750, 0 },
    { -4796, -2240, -1750, 0 },
    { -4775, -2254, -1750, 0 },
    { -4754, -2268, -1750, 0 },
    { -4733, -2282, -1750, 0 },
    { -4712, -2296, -1750, 0 },
    { -4691, -2310, -1750, 0 },
    { -4669, -2325, -1750, 0 },
    { -4648, -2339, -1750, 0 },
    { -4627, -2353, -1750, 0 },
    { -4606, -2367, -1750, 0 },
    { -4585, -2381, -1750, 0 },
    { -4564, -2395, -1750, 0 },
    { -4543, -2409, -1750, 0 },
    { -4521, -2423, -1750, 0 },
    { -4500, -2437, -1750, 0 },
    { -4479, -2451, -1750, 0 },
    { -4458, -2466, -1750, 0 },
    { -4437, -2480, -1750, 0 },
    { -4416, -2494, -1750, 0 },
    { -4394, -2508, -1750, 0 },
    { -4373, -2522, -1750, 0 },
    { -4352, -2536, -1750, 0 },
    { -4331, -2550, -1750, 0 },
    { -4310, -2564, -1750, 0 },
    { -4289, -2578, -1750, 0 },
    { -4268, -2592, -1750, 0 },
    { -4246, -2607, -1750, 0 },
    { -4225, -2621, -1750, 0 },
    { -4204, -2635, -1750, 0 },
    { -4183, -2649, -1750, 0 },
    { -4162, -2663, -1750, 0 },
    { -4141, -2677, -1750, 0 },
    { -4119, -2691, -1750, 0 },
    { -4098, -2705, -1750, 0 },
    { -4077, -2719, -1750, 0 },
    { -4056, -2734, -1750, 0 },
    { -4035, -2748, -1750, 0 },
    { -4014, -2762, -1750, 0 },
    { -3992, -2776, -1750, 0 },
    { -3971, -2790, -1750, 0 },
    { -3950, -2804, -1750, 0 },
    { -3929, -2818, -1750, 0 },
    { -3908, -2832, -1750, 0 },
    { -3887, -2846, -1750, 0 },
    { -3866, -2860, -1750, 0 },
    { -3844, -2875, -1750, 0 },
    { -3823, -2889, -1750, 0 },
    { -3802, -2903, -1750, 0 },
    { -3781, -2917, -1750, 0 },
    { -3760, -2931, -1750, 0 },
    { -3739, -2945, -1750, 0 },
    { -3717, -2959, -1750, 0 },
    { -3696, -2973, -1750, 0 },
    { -3675, -2987, -1750, 0 },
    { -3652, -2995, -1750, 0 },
    { -3626, -2995, -1750, 0 },
    { -3601, -2995, -1750, 0 },
    { -3576, -2995, -1750, 0 },
    { -3550, -2995, -1750, 0 },
    { -3525, -2995, -1750, 0 },
    { -3499, -2995, -1750, 0 },
    { -3474, -2995, -1750, 0 },
    { -3448, -2995, -1750, 0 },
    { -3423, -2995, -1750, 0 },
    { -3398, -2995, -1750, 0 },
    { -3372, -2995, -1750, 0 },
    { -3347, -2995, -1750, 0 },
    { -3321, -2995, -1750, 0 },
    { -3296, -2995, -1750, 0 },
    { -3270, -2995, -1750, 0 },
    { -3245, -2995, -1750, 0 },
    { -3220, -2995, -1750, 0 },
    { -3194, -2995, -1750, 0 },
    { -3169, -2995, -1750, 0 },
    { -3143, -2995, -1750, 0 },
    { -3118, -2995, -1750, 0 },
    { -3092, -2995, -1750, 0 },
    { -3067, -2995, -1750, 0 },
    { -3042, -2995, -1750, 0 },
    { -3016, -2995, -1750, 0 },
    { -2991, -2995, -1750, 0 },
    { -2965, -2995, -1750, 0 },
    { -2940, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
    { -2915, -2995, -1750, 0 },
};

SVECTOR D_acropolis_observatory_8017F16C[300] = {
    { -8465, 5, -0x29FE, 0 },
    { -8439, 5, -0x29FE, 0 },
    { -8414, 5, -0x29FE, 0 },
    { -8388, 5, -0x29FE, 0 },
    { -8363, 5, -0x29FE, 0 },
    { -8337, 5, -0x29FE, 0 },
    { -8312, 5, -0x29FE, 0 },
    { -8287, 5, -0x29FE, 0 },
    { -8261, 5, -0x29FE, 0 },
    { -8236, 5, -0x29FE, 0 },
    { -8210, 5, -0x29FE, 0 },
    { -8185, 5, -0x29FE, 0 },
    { -8160, 2, -0x29FE, 0 },
    { -8139, -11, -0x29FE, 0 },
    { -8118, -26, -0x29FE, 0 },
    { -8097, -40, -0x29FE, 0 },
    { -8076, -54, -0x29FE, 0 },
    { -8054, -68, -0x29FE, 0 },
    { -8033, -82, -0x29FE, 0 },
    { -8012, -96, -0x29FE, 0 },
    { -7991, -110, -0x29FE, 0 },
    { -7970, -124, -0x29FE, 0 },
    { -7949, -138, -0x29FE, 0 },
    { -7928, -152, -0x29FE, 0 },
    { -7906, -167, -0x29FE, 0 },
    { -7885, -181, -0x29FE, 0 },
    { -7864, -195, -0x29FE, 0 },
    { -7843, -209, -0x29FE, 0 },
    { -7822, -223, -0x29FE, 0 },
    { -7801, -237, -0x29FE, 0 },
    { -7779, -251, -0x29FE, 0 },
    { -7758, -265, -0x29FE, 0 },
    { -7737, -279, -0x29FE, 0 },
    { -7716, -294, -0x29FE, 0 },
    { -7695, -308, -0x29FE, 0 },
    { -7674, -322, -0x29FE, 0 },
    { -7652, -336, -0x29FE, 0 },
    { -7631, -350, -0x29FE, 0 },
    { -7610, -364, -0x29FE, 0 },
    { -7589, -378, -0x29FE, 0 },
    { -7568, -392, -0x29FE, 0 },
    { -7547, -406, -0x29FE, 0 },
    { -7526, -420, -0x29FE, 0 },
    { -7504, -435, -0x29FE, 0 },
    { -7483, -449, -0x29FE, 0 },
    { -7462, -463, -0x29FE, 0 },
    { -7441, -477, -0x29FE, 0 },
    { -7420, -491, -0x29FE, 0 },
    { -7399, -505, -0x29FE, 0 },
    { -7377, -519, -0x29FE, 0 },
    { -7356, -533, -0x29FE, 0 },
    { -7335, -547, -0x29FE, 0 },
    { -7314, -562, -0x29FE, 0 },
    { -7293, -576, -0x29FE, 0 },
    { -7272, -590, -0x29FE, 0 },
    { -7251, -604, -0x29FE, 0 },
    { -7229, -618, -0x29FE, 0 },
    { -7208, -632, -0x29FE, 0 },
    { -7187, -646, -0x29FE, 0 },
    { -7166, -660, -0x29FE, 0 },
    { -7145, -674, -0x29FE, 0 },
    { -7124, -688, -0x29FE, 0 },
    { -7102, -703, -0x29FE, 0 },
    { -7081, -717, -0x29FE, 0 },
    { -7060, -731, -0x29FE, 0 },
    { -7039, -745, -0x29FE, 0 },
    { -7018, -759, -0x29FE, 0 },
    { -6997, -773, -0x29FE, 0 },
    { -6975, -787, -0x29FE, 0 },
    { -6954, -801, -0x29FE, 0 },
    { -6933, -815, -0x29FE, 0 },
    { -6912, -829, -0x29FE, 0 },
    { -6891, -844, -0x29FE, 0 },
    { -6870, -858, -0x29FE, 0 },
    { -6849, -872, -0x29FE, 0 },
    { -6827, -886, -0x29FE, 0 },
    { -6806, -900, -0x29FE, 0 },
    { -6785, -914, -0x29FE, 0 },
    { -6764, -928, -0x29FE, 0 },
    { -6743, -942, -0x29FE, 0 },
    { -6722, -956, -0x29FE, 0 },
    { -6700, -971, -0x29FE, 0 },
    { -6679, -985, -0x29FE, 0 },
    { -6658, -999, -0x29FE, 0 },
    { -6637, -1013, -0x29FE, 0 },
    { -6616, -1027, -0x29FE, 0 },
    { -6595, -1041, -0x29FE, 0 },
    { -6574, -1055, -0x29FE, 0 },
    { -6552, -1069, -0x29FE, 0 },
    { -6531, -1083, -0x29FE, 0 },
    { -6510, -1097, -0x29FE, 0 },
    { -6489, -1112, -0x29FE, 0 },
    { -6468, -1126, -0x29FE, 0 },
    { -6447, -1140, -0x29FE, 0 },
    { -6425, -1154, -0x29FE, 0 },
    { -6404, -1168, -0x29FE, 0 },
    { -6383, -1182, -0x29FE, 0 },
    { -6362, -1196, -0x29FE, 0 },
    { -6341, -1210, -0x29FE, 0 },
    { -6320, -1224, -0x29FE, 0 },
    { -6299, -1238, -0x29FE, 0 },
    { -6277, -1253, -0x29FE, 0 },
    { -6256, -1267, -0x29FE, 0 },
    { -6235, -1281, -0x29FE, 0 },
    { -6214, -1295, -0x29FE, 0 },
    { -6193, -1309, -0x29FE, 0 },
    { -6172, -1323, -0x29FE, 0 },
    { -6150, -1337, -0x29FE, 0 },
    { -6129, -1351, -0x29FE, 0 },
    { -6108, -1365, -0x29FE, 0 },
    { -6087, -1380, -0x29FE, 0 },
    { -6066, -1394, -0x29FE, 0 },
    { -6045, -1408, -0x29FE, 0 },
    { -6023, -1422, -0x29FE, 0 },
    { -6002, -1436, -0x29FE, 0 },
    { -5981, -1450, -0x29FE, 0 },
    { -5960, -1464, -0x29FE, 0 },
    { -5939, -1478, -0x29FE, 0 },
    { -5918, -1492, -0x29FE, 0 },
    { -5897, -1506, -0x29FE, 0 },
    { -5875, -1521, -0x29FE, 0 },
    { -5854, -1535, -0x29FE, 0 },
    { -5833, -1549, -0x29FE, 0 },
    { -5812, -1563, -0x29FE, 0 },
    { -5791, -1577, -0x29FE, 0 },
    { -5770, -1591, -0x29FE, 0 },
    { -5748, -1605, -0x29FE, 0 },
    { -5727, -1619, -0x29FE, 0 },
    { -5706, -1633, -0x29FE, 0 },
    { -5685, -1648, -0x29FE, 0 },
    { -5664, -1662, -0x29FE, 0 },
    { -5643, -1676, -0x29FE, 0 },
    { -5622, -1690, -0x29FE, 0 },
    { -5600, -1704, -0x29FE, 0 },
    { -5579, -1718, -0x29FE, 0 },
    { -5558, -1732, -0x29FE, 0 },
    { -5537, -1746, -0x29FE, 0 },
    { -5516, -1760, -0x29FE, 0 },
    { -5495, -1774, -0x29FE, 0 },
    { -5473, -1789, -0x29FE, 0 },
    { -5452, -1803, -0x29FE, 0 },
    { -5431, -1817, -0x29FE, 0 },
    { -5410, -1831, -0x29FE, 0 },
    { -5389, -1845, -0x29FE, 0 },
    { -5368, -1859, -0x29FE, 0 },
    { -5346, -1873, -0x29FE, 0 },
    { -5325, -1887, -0x29FE, 0 },
    { -5304, -1901, -0x29FE, 0 },
    { -5283, -1915, -0x29FE, 0 },
    { -5262, -1930, -0x29FE, 0 },
    { -5241, -1944, -0x29FE, 0 },
    { -5220, -1958, -0x29FE, 0 },
    { -5198, -1972, -0x29FE, 0 },
    { -5177, -1986, -0x29FE, 0 },
    { -5156, -2000, -0x29FE, 0 },
    { -5135, -2014, -0x29FE, 0 },
    { -5114, -2028, -0x29FE, 0 },
    { -5093, -2042, -0x29FE, 0 },
    { -5071, -2057, -0x29FE, 0 },
    { -5050, -2071, -0x29FE, 0 },
    { -5029, -2085, -0x29FE, 0 },
    { -5008, -2099, -0x29FE, 0 },
    { -4987, -2113, -0x29FE, 0 },
    { -4966, -2127, -0x29FE, 0 },
    { -4945, -2141, -0x29FE, 0 },
    { -4923, -2155, -0x29FE, 0 },
    { -4902, -2169, -0x29FE, 0 },
    { -4881, -2183, -0x29FE, 0 },
    { -4860, -2198, -0x29FE, 0 },
    { -4839, -2212, -0x29FE, 0 },
    { -4818, -2226, -0x29FE, 0 },
    { -4796, -2240, -0x29FE, 0 },
    { -4775, -2254, -0x29FE, 0 },
    { -4754, -2268, -0x29FE, 0 },
    { -4733, -2282, -0x29FE, 0 },
    { -4712, -2296, -0x29FE, 0 },
    { -4691, -2310, -0x29FE, 0 },
    { -4669, -2325, -0x29FE, 0 },
    { -4648, -2339, -0x29FE, 0 },
    { -4627, -2353, -0x29FE, 0 },
    { -4606, -2367, -0x29FE, 0 },
    { -4585, -2381, -0x29FE, 0 },
    { -4564, -2395, -0x29FE, 0 },
    { -4543, -2409, -0x29FE, 0 },
    { -4521, -2423, -0x29FE, 0 },
    { -4500, -2437, -0x29FE, 0 },
    { -4479, -2451, -0x29FE, 0 },
    { -4458, -2466, -0x29FE, 0 },
    { -4437, -2480, -0x29FE, 0 },
    { -4416, -2494, -0x29FE, 0 },
    { -4394, -2508, -0x29FE, 0 },
    { -4373, -2522, -0x29FE, 0 },
    { -4352, -2536, -0x29FE, 0 },
    { -4331, -2550, -0x29FE, 0 },
    { -4310, -2564, -0x29FE, 0 },
    { -4289, -2578, -0x29FE, 0 },
    { -4268, -2592, -0x29FE, 0 },
    { -4246, -2607, -0x29FE, 0 },
    { -4225, -2621, -0x29FE, 0 },
    { -4204, -2635, -0x29FE, 0 },
    { -4183, -2649, -0x29FE, 0 },
    { -4162, -2663, -0x29FE, 0 },
    { -4141, -2677, -0x29FE, 0 },
    { -4119, -2691, -0x29FE, 0 },
    { -4098, -2705, -0x29FE, 0 },
    { -4077, -2719, -0x29FE, 0 },
    { -4056, -2734, -0x29FE, 0 },
    { -4035, -2748, -0x29FE, 0 },
    { -4014, -2762, -0x29FE, 0 },
    { -3992, -2776, -0x29FE, 0 },
    { -3971, -2790, -0x29FE, 0 },
    { -3950, -2804, -0x29FE, 0 },
    { -3929, -2818, -0x29FE, 0 },
    { -3908, -2832, -0x29FE, 0 },
    { -3887, -2846, -0x29FE, 0 },
    { -3866, -2860, -0x29FE, 0 },
    { -3844, -2875, -0x29FE, 0 },
    { -3823, -2889, -0x29FE, 0 },
    { -3802, -2903, -0x29FE, 0 },
    { -3781, -2917, -0x29FE, 0 },
    { -3760, -2931, -0x29FE, 0 },
    { -3739, -2945, -0x29FE, 0 },
    { -3717, -2959, -0x29FE, 0 },
    { -3696, -2973, -0x29FE, 0 },
    { -3675, -2987, -0x29FE, 0 },
    { -3652, -2995, -0x29FE, 0 },
    { -3626, -2995, -0x29FE, 0 },
    { -3601, -2995, -0x29FE, 0 },
    { -3576, -2995, -0x29FE, 0 },
    { -3550, -2995, -0x29FE, 0 },
    { -3525, -2995, -0x29FE, 0 },
    { -3499, -2995, -0x29FE, 0 },
    { -3474, -2995, -0x29FE, 0 },
    { -3448, -2995, -0x29FE, 0 },
    { -3423, -2995, -0x29FE, 0 },
    { -3398, -2995, -0x29FE, 0 },
    { -3372, -2995, -0x29FE, 0 },
    { -3347, -2995, -0x29FE, 0 },
    { -3321, -2995, -0x29FE, 0 },
    { -3296, -2995, -0x29FE, 0 },
    { -3270, -2995, -0x29FE, 0 },
    { -3245, -2995, -0x29FE, 0 },
    { -3220, -2995, -0x29FE, 0 },
    { -3194, -2995, -0x29FE, 0 },
    { -3169, -2995, -0x29FE, 0 },
    { -3143, -2995, -0x29FE, 0 },
    { -3118, -2995, -0x29FE, 0 },
    { -3092, -2995, -0x29FE, 0 },
    { -3067, -2995, -0x29FE, 0 },
    { -3042, -2995, -0x29FE, 0 },
    { -3016, -2995, -0x29FE, 0 },
    { -2991, -2995, -0x29FE, 0 },
    { -2965, -2995, -0x29FE, 0 },
    { -2940, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
    { -2915, -2995, -0x29FE, 0 },
};

static AnimationPackedPose _gAcropolisObservatoryAnimation02878Bank1[7] = {
#include "assets/acropolis_observatory_animation_02878_bank1.inc"
};

static AnimationPackedRotation _gAcropolisObservatoryAnimation02878Bank4[65] = {
#include "assets/acropolis_observatory_animation_02878_bank4.inc"
};

static AnimationRecord _gAcropolisObservatoryAnimation02878Records[123] = {
#include "assets/acropolis_observatory_animation_02878_records.inc"
};

static u16 _gAcropolisObservatoryAnimation02878Indices[20] = {
#include "assets/acropolis_observatory_animation_02878_indices.inc"
};

AnimationSet gAcropolisObservatoryAnimation02878 = {
    _gAcropolisObservatoryAnimation02878Records,
    _gAcropolisObservatoryAnimation02878Indices,
    { NULL, _gAcropolisObservatoryAnimation02878Bank1, NULL, NULL, _gAcropolisObservatoryAnimation02878Bank4, NULL, NULL, NULL },
};

AnimationSet* gAcropolisObservatoryPlayerAnimationSets[2] = { NULL, &gAcropolisObservatoryAnimation02878 };

/// Streamed-scene ride, entry 0 of the room's task table: the same ride as
/// `func_acropolis_observatory_8017DD3C` (entry 1), walking the player's matrix
/// along `D_acropolis_observatory_8017E80C` instead and ending on view index 2.
/// State 0 allocates the `RoomMoviePathWork` block, cues the stream (slot-6 msg
/// 0xFA4), captures slot 3 and the player's coordinate matrix and republishes
/// the player's weapon to slot 3 with a 0x3E8 record. State 1 waits for the
/// stream (`gCdCmdQueue::movieReady`), starts the script pair and adopts its
/// task as a child. State 2 drives the ride, letting the pad skip it once
/// through the fade-out task and warping slot 3 when that task has finished
/// or frame 0xE6 passes.
/// State 3 waits for slot 3 to go idle, releases it and records the view.
/// State 4 stops the stream and kills the task.
void func_acropolis_observatory_8017D9A8(Task* task)
{
    AnimationPlayRequest rec;
    ActorTransform       place;
    s32                  killed;
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
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            rec.source.index                             = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId                              = 1;
            rec.blend                                    = ANIMATION_BLEND_RESET;
            rec.blendFrames                              = 0;
            rec.enableWorldCollision                     = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            func_800E9BDC(3, 0x9FF);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->padScriptTask           = Gp_SpawnScript18(D_acropolis_observatory_80183480,
                                                                 D_acropolis_observatory_80183498);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->playerMtx->t[0] = D_acropolis_observatory_8017E80C[queue->movieFrame + 0xA8].vx;
            work->playerMtx->t[1] = D_acropolis_observatory_8017E80C[queue->movieFrame + 0xA8].vy;
            work->playerMtx->t[2] = D_acropolis_observatory_8017E80C[queue->movieFrame + 0xA8].vz;
            if (work->skipFadeStarted != 0) {
                if (Task_PollKill(work->skipFadeTask, &killed) != 0) {
                    place.pos.vx = -0x968;
                    place.pos.vy = -0xBAD;
                    place.pos.vz = -0x6D4;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0x400;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
                    taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 3, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 2, 0, 0);
                work->skipFadeStarted = 1;
            }
            if ((queue->movieFrame + 0xA8) >= 0xE6) {
                place.pos.vx = -0x968;
                place.pos.vy = -0xBAD;
                place.pos.vz = -0x6D4;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = Gp_FindViewIndex(2);
                task->state                                                = task->state + 1;
            }
            break;

        case 4:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
            func_800E9BDC(2, 0x9FF);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Streamed-scene ride, entry 1 of the room's task table. State 0 allocates the
/// `RoomMoviePathWork` block, cues the stream (slot-6 msg 0xFA4), captures slot 3
/// and the player's coordinate matrix in the block, and republishes the
/// player's weapon to slot 3 with a 0x3E8 record. State 1 waits for the stream
/// to come up (`gCdCmdQueue::movieReady`), then starts the script pair and
/// adopts its task as a child. State 2 drives the ride: every frame it moves
/// the player's matrix to the `field_1EA`th entry of the path table; the first
/// time `Pad_CheckFlag800` reports the pad it spawns the fade-out task (entry 2
/// of the room's task table), and once that task has finished it warps slot 3
/// with a 0x3E9 placement and spawns the fade-in (entry 3); past frame 0xE6 it
/// sends the same placement as a
/// 0x3F2 and moves on either way. State 3 waits for slot 3 to go idle (msg
/// 0x3F0), releases it (0x3F1) and records the view in the save. State 4 stops
/// the stream (0xFA5), clears the scene flags and kills the task.
void func_acropolis_observatory_8017DD3C(Task* task)
{
    AnimationPlayRequest rec;
    ActorTransform       place;
    s32                  killed;
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
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            weaponId                                     = gPlayerStatus.weapon;
            rec.source.index                             = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animationId                              = 1;
            rec.blend                                    = ANIMATION_BLEND_RESET;
            rec.blendFrames                              = 0;
            rec.enableWorldCollision                     = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            func_800E9BDC(3, 0x9FF);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state                    = task->state + 1;
            break;

        case 1:
            if (queue->movieReady != 0) {
                work->padScriptTask           = Gp_SpawnScript18(D_acropolis_observatory_801834A0,
                                                                 D_acropolis_observatory_801834B8);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, work->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->playerMtx->t[0] = D_acropolis_observatory_8017F16C[queue->movieFrame + 0xA8].vx;
            work->playerMtx->t[1] = D_acropolis_observatory_8017F16C[queue->movieFrame + 0xA8].vy;
            work->playerMtx->t[2] = D_acropolis_observatory_8017F16C[queue->movieFrame + 0xA8].vz + 0xC8;
            if (work->skipFadeStarted != 0) {
                if (Task_PollKill(work->skipFadeTask, &killed) != 0) {
                    place.pos.vx = -0x8F8;
                    place.pos.vy = -0xBAD;
                    place.pos.vz = -0x2936;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0x400;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3E9, &place, 0);
                    taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 3, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->skipFadeTask    = taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 2, 0, 0);
                work->skipFadeStarted = 1;
            }
            if ((queue->movieFrame + 0xA8) >= 0xE6) {
                place.pos.vx = -0x8F8;
                place.pos.vy = -0xBAD;
                place.pos.vz = -0x2936;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, 0x3F2, &place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = Gp_FindViewIndex(4);
                task->state                                                = task->state + 1;
            }
            break;

        case 4:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
            func_800E9BDC(2, 0x9FF);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Fade-out task, entry 2 of the room's task table: subtracts a full-screen
/// grey that grows by 0x20 a frame, counted in `Task::killCountdown`, and asks
/// for its own release once the screen is black. The streamed-scene tasks
/// wait on that release before warping the player.
void func_acropolis_observatory_8017E0D4(Task* arg0)
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

/// Fade-in task, entry 3 of the room's task table: the reverse of
/// `func_acropolis_observatory_8017E0D4`, subtracting a grey that shrinks by
/// 0x20 a frame from black, then killing itself.
void func_acropolis_observatory_8017E134(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = ~(u8)arg0->killCountdown;
    fadeDrawOverlay(fade, fade, fade, GPU_BLEND_SUBTRACT);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
