#include "actors/actor_503500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_messages.h"

/// Work block of a slider: one of the two long slab models the Brahman
/// intro script slides into place along a precomputed path while the screen
/// shakes.
///
/// It is allocated zeroed and kept at `Task::work`. The two matrices are the
/// model's own light and colour pair, bound to `TmdObject::lightMtx` /
/// `colorMtx` in place of the shared defaults.
typedef struct {
    MATRIX light;         // Light matrix the model is lit with
    MATRIX color;         // Colour matrix paired with `light`
    s16    timer;         // Path step, counting up, while `path` is set; otherwise frames of shaking left, counting down
    byte   pad_42[0x2];
    s8     freeCountdown; // Frames until the model's buffers are freed; negative once that is done
    s8     path;          // Path being followed (0 none, 1 first table, 2 second table)
    byte   pad_46[0x2];
} _Actor503500SliderWork;
STATIC_ASSERT_SIZEOF(_Actor503500SliderWork, 0x48);

static void func_actor_503500_801324C4(Task* task);
static void func_actor_503500_801324EC(Task* arg0);
/// Script pair handed to `Gp_SpawnScript18` on every odd pulse frame.
extern PadScriptCmd              D_actor_503500_801468A8[2];
extern PadScriptVibrationSegment D_actor_503500_801468B0[2];
/// Two 360-entry X/Z paths `func_actor_503500_8013223C` walks the model along,
/// selected by `_Actor503500SliderWork::path` (1 or 2).
extern DVECTOR_XZ D_actor_503500_80147D90[];
extern DVECTOR_XZ D_actor_503500_80148330[];

static u32     _gActor503500Model15820PartVerts[1];
static SVECTOR _gActor503500Model15820Verts[92];
static TmdBone _gActor503500Model15820Skeleton[1];
static u32     _gActor503500Model15820Stream[459];

s32 func_actor_503500_80132584(Task*, s32, s32, s32);
s32 func_actor_503500_80132664(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

TaskMessageEntry D_actor_503500_80146888[4] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_503500_80132584 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_503500_80132664 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

PadScriptCmd D_actor_503500_801468A8[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_801468B0[2] = { { 0, 0, 2, 0 }, { 156, 106, 2, 1 } };

static TmdBone _gActor503500Model14DA0Skeleton[1] = {
#include "assets/actor_503500_model_14DA0_skeleton.inc"
};

static u32 _gActor503500Model14DA0PartVerts[1] = {
#include "assets/actor_503500_model_14DA0_partVerts.inc"
};

static SVECTOR _gActor503500Model14DA0Verts[92] = {
#include "assets/actor_503500_model_14DA0_verts.inc"
};

static u32 _gActor503500Model14DA0Stream[469] = {
#include "assets/actor_503500_model_14DA0_stream.inc"
};

TmdSource gActor503500Model14DA0 = {
    0,
    3600,
    0,
    1,
    _gActor503500Model14DA0PartVerts,
    _gActor503500Model14DA0Verts,
    &_gActor503500Model14DA0Verts[92],
    _gActor503500Model14DA0Skeleton,
    _gActor503500Model14DA0Stream,
};

static TmdBone _gActor503500Model15820Skeleton[1] = {
#include "assets/actor_503500_model_15820_skeleton.inc"
};

static u32 _gActor503500Model15820PartVerts[1] = {
#include "assets/actor_503500_model_15820_partVerts.inc"
};

static SVECTOR _gActor503500Model15820Verts[92] = {
#include "assets/actor_503500_model_15820_verts.inc"
};

static u32 _gActor503500Model15820Stream[459] = {
#include "assets/actor_503500_model_15820_stream.inc"
};

TmdSource gActor503500Model15820 = {
    0,
    3552,
    0,
    1,
    _gActor503500Model15820PartVerts,
    _gActor503500Model15820Verts,
    &_gActor503500Model15820Verts[92],
    _gActor503500Model15820Skeleton,
    _gActor503500Model15820Stream,
};

DVECTOR_XZ D_actor_503500_80147D90[360] = {
    { 3350, 0x2D82 },
    { 3369, 0x2D6E },
    { 3389, 0x2D5A },
    { 3408, 0x2D47 },
    { 3428, 0x2D33 },
    { 3448, 0x2D1F },
    { 3467, 0x2D0C },
    { 3487, 0x2CF8 },
    { 3506, 0x2CE5 },
    { 3526, 0x2CD1 },
    { 3546, 0x2CBD },
    { 3565, 0x2CAA },
    { 3585, 0x2C96 },
    { 3604, 0x2C83 },
    { 3624, 0x2C6F },
    { 3643, 0x2C5C },
    { 3663, 0x2C48 },
    { 3683, 0x2C34 },
    { 3702, 0x2C21 },
    { 3722, 0x2C0D },
    { 3741, 0x2BFA },
    { 3761, 0x2BE6 },
    { 3780, 0x2BD3 },
    { 3800, 0x2BBF },
    { 3819, 0x2BAC },
    { 3839, 0x2B98 },
    { 3858, 0x2B85 },
    { 3877, 0x2B72 },
    { 3897, 0x2B5E },
    { 3916, 0x2B4B },
    { 3936, 0x2B37 },
    { 3955, 0x2B24 },
    { 3975, 0x2B10 },
    { 3994, 0x2AFD },
    { 4013, 0x2AEA },
    { 4033, 0x2AD6 },
    { 4052, 0x2AC3 },
    { 4072, 0x2AAF },
    { 4091, 0x2A9C },
    { 4110, 0x2A89 },
    { 4129, 0x2A76 },
    { 4149, 0x2A62 },
    { 4168, 0x2A4F },
    { 4187, 0x2A3C },
    { 4207, 0x2A28 },
    { 4226, 0x2A15 },
    { 4245, 0x2A02 },
    { 4264, 0x29EF },
    { 4284, 0x29DB },
    { 4303, 0x29C8 },
    { 4322, 0x29B5 },
    { 4341, 0x29A2 },
    { 4360, 0x298F },
    { 4379, 0x297C },
    { 4398, 0x2969 },
    { 4417, 0x2956 },
    { 4437, 0x2942 },
    { 4456, 0x292F },
    { 4475, 0x291C },
    { 4494, 0x2909 },
    { 4513, 0x28F6 },
    { 4532, 0x28E3 },
    { 4551, 0x28D0 },
    { 4570, 0x28BD },
    { 4589, 0x28AA },
    { 4607, 0x2898 },
    { 4626, 0x2885 },
    { 4645, 0x2872 },
    { 4664, 0x285F },
    { 4683, 0x284C },
    { 4702, 0x2839 },
    { 4720, 0x2827 },
    { 4739, 0x2814 },
    { 4758, 0x2801 },
    { 4777, 0x27EE },
    { 4795, 0x27DC },
    { 4814, 0x27C9 },
    { 4833, 0x27B6 },
    { 4851, 0x27A4 },
    { 4870, 0x2791 },
    { 4889, 0x277E },
    { 4907, 0x276C },
    { 4926, 0x2759 },
    { 4944, 0x2747 },
    { 4963, 0x2734 },
    { 4981, 0x2722 },
    { 5000, 9999 },
    { 5018, 9981 },
    { 5037, 9962 },
    { 5055, 9944 },
    { 5073, 9926 },
    { 5092, 9907 },
    { 5110, 9889 },
    { 5128, 9871 },
    { 5146, 9853 },
    { 5165, 9834 },
    { 5183, 9816 },
    { 5201, 9798 },
    { 5219, 9780 },
    { 5237, 9762 },
    { 5255, 9744 },
    { 5273, 9726 },
    { 5292, 9707 },
    { 5310, 9689 },
    { 5327, 9672 },
    { 5345, 9654 },
    { 5363, 9636 },
    { 5381, 9618 },
    { 5399, 9600 },
    { 5417, 9582 },
    { 5435, 9564 },
    { 5452, 9547 },
    { 5470, 9529 },
    { 5488, 9511 },
    { 5506, 9493 },
    { 5523, 9476 },
    { 5541, 9458 },
    { 5558, 9441 },
    { 5576, 9423 },
    { 5593, 9406 },
    { 5611, 9388 },
    { 5628, 9371 },
    { 5646, 9353 },
    { 5663, 9336 },
    { 5681, 9318 },
    { 5698, 9301 },
    { 5715, 9284 },
    { 5732, 9267 },
    { 5750, 9249 },
    { 5767, 9232 },
    { 5784, 9215 },
    { 5801, 9198 },
    { 5818, 9181 },
    { 5835, 9164 },
    { 5852, 9147 },
    { 5869, 9130 },
    { 5886, 9113 },
    { 5903, 9096 },
    { 5920, 9079 },
    { 5936, 9063 },
    { 5953, 9046 },
    { 5970, 9029 },
    { 5987, 9012 },
    { 6003, 8996 },
    { 6020, 8979 },
    { 6037, 8962 },
    { 6053, 8946 },
    { 6070, 8929 },
    { 6086, 8913 },
    { 6102, 8897 },
    { 6119, 8880 },
    { 6135, 8864 },
    { 6151, 8848 },
    { 6168, 8831 },
    { 6184, 8815 },
    { 6200, 8799 },
    { 6216, 8783 },
    { 6232, 8767 },
    { 6248, 8751 },
    { 6264, 8735 },
    { 6280, 8719 },
    { 6296, 8703 },
    { 6312, 8687 },
    { 6328, 8671 },
    { 6344, 8655 },
    { 6359, 8640 },
    { 6375, 8624 },
    { 6391, 8608 },
    { 6406, 8593 },
    { 6422, 8577 },
    { 6437, 8562 },
    { 6453, 8546 },
    { 6468, 8531 },
    { 6484, 8515 },
    { 6499, 8500 },
    { 6514, 8485 },
    { 6530, 8469 },
    { 6545, 8454 },
    { 6560, 8439 },
    { 6575, 8424 },
    { 6590, 8409 },
    { 6605, 8394 },
    { 6620, 8379 },
    { 6635, 8364 },
    { 6650, 8349 },
    { 6665, 8334 },
    { 6679, 8320 },
    { 6694, 8305 },
    { 6709, 8290 },
    { 6723, 8276 },
    { 6738, 8261 },
    { 6752, 8247 },
    { 6767, 8232 },
    { 6781, 8218 },
    { 6796, 8203 },
    { 6810, 8189 },
    { 6824, 8175 },
    { 6838, 8161 },
    { 6853, 8146 },
    { 6867, 8132 },
    { 6881, 8118 },
    { 6895, 8104 },
    { 6909, 8090 },
    { 6922, 8077 },
    { 6936, 8063 },
    { 6950, 8049 },
    { 6964, 8035 },
    { 6977, 8022 },
    { 6991, 8008 },
    { 7005, 7994 },
    { 7018, 7981 },
    { 7032, 7967 },
    { 7045, 7954 },
    { 7058, 7941 },
    { 7072, 7927 },
    { 7085, 7914 },
    { 7098, 7901 },
    { 7111, 7888 },
    { 7124, 7875 },
    { 7137, 7862 },
    { 7150, 7849 },
    { 7163, 7836 },
    { 7176, 7823 },
    { 7188, 7811 },
    { 7201, 7798 },
    { 7214, 7785 },
    { 7226, 7773 },
    { 7239, 7760 },
    { 7251, 7748 },
    { 7264, 7735 },
    { 7276, 7723 },
    { 7288, 7711 },
    { 7300, 7699 },
    { 7313, 7686 },
    { 7325, 7674 },
    { 7337, 7662 },
    { 7349, 7650 },
    { 7361, 7638 },
    { 7372, 7627 },
    { 7384, 7615 },
    { 7396, 7603 },
    { 7408, 7591 },
    { 7419, 7580 },
    { 7431, 7568 },
    { 7442, 7557 },
    { 7454, 7545 },
    { 7465, 7534 },
    { 7476, 7523 },
    { 7487, 7512 },
    { 7499, 7500 },
    { 7510, 7489 },
    { 7520, 7479 },
    { 7531, 7468 },
    { 7542, 7457 },
    { 7552, 7447 },
    { 7562, 7437 },
    { 7572, 7427 },
    { 7582, 7417 },
    { 7592, 7407 },
    { 7601, 7398 },
    { 7610, 7389 },
    { 7620, 7379 },
    { 7629, 7370 },
    { 7637, 7362 },
    { 7646, 7353 },
    { 7655, 7344 },
    { 7663, 7336 },
    { 7671, 7328 },
    { 7679, 7320 },
    { 7687, 7312 },
    { 7695, 7304 },
    { 7703, 7296 },
    { 7710, 7289 },
    { 7718, 7281 },
    { 7725, 7274 },
    { 7732, 7267 },
    { 7739, 7260 },
    { 7746, 7253 },
    { 7752, 7247 },
    { 7759, 7240 },
    { 7765, 7234 },
    { 7772, 7227 },
    { 7778, 7221 },
    { 7784, 7215 },
    { 7790, 7209 },
    { 7795, 7204 },
    { 7801, 7198 },
    { 7806, 7193 },
    { 7812, 7187 },
    { 7817, 7182 },
    { 7822, 7177 },
    { 7827, 7172 },
    { 7832, 7167 },
    { 7837, 7162 },
    { 7841, 7158 },
    { 7846, 7153 },
    { 7850, 7149 },
    { 7855, 7144 },
    { 7859, 7140 },
    { 7863, 7136 },
    { 7867, 7132 },
    { 7871, 7128 },
    { 7874, 7125 },
    { 7878, 7121 },
    { 7882, 7117 },
    { 7885, 7114 },
    { 7889, 7110 },
    { 7892, 7107 },
    { 7895, 7104 },
    { 7898, 7101 },
    { 7901, 7098 },
    { 7904, 7095 },
    { 7907, 7092 },
    { 7909, 7090 },
    { 7912, 7087 },
    { 7915, 7084 },
    { 7917, 7082 },
    { 7919, 7080 },
    { 7922, 7077 },
    { 7924, 7075 },
    { 7926, 7073 },
    { 7928, 7071 },
    { 7930, 7069 },
    { 7932, 7067 },
    { 7934, 7065 },
    { 7935, 7064 },
    { 7937, 7062 },
    { 7938, 7061 },
    { 7940, 7059 },
    { 7941, 7058 },
    { 7943, 7056 },
    { 7944, 7055 },
    { 7945, 7054 },
    { 7947, 7052 },
    { 7948, 7051 },
    { 7949, 7050 },
    { 7950, 7049 },
    { 7951, 7048 },
    { 7952, 7047 },
    { 7952, 7047 },
    { 7953, 7046 },
    { 7954, 7045 },
    { 7955, 7044 },
    { 7955, 7044 },
    { 7956, 7043 },
    { 7956, 7043 },
    { 7957, 7042 },
    { 7957, 7042 },
    { 7958, 7041 },
    { 7958, 7041 },
    { 7958, 7041 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7959, 7040 },
    { 7960, 7040 },
};

DVECTOR_XZ D_actor_503500_80148330[360] = {
    { 0x311A, 2430 },
    { 0x3106, 2449 },
    { 0x30F2, 2469 },
    { 0x30DF, 2488 },
    { 0x30CB, 2508 },
    { 0x30B7, 2528 },
    { 0x30A4, 2547 },
    { 0x3090, 2567 },
    { 0x307C, 2587 },
    { 0x3069, 2606 },
    { 0x3055, 2626 },
    { 0x3042, 2645 },
    { 0x302E, 2665 },
    { 0x301A, 2685 },
    { 0x3007, 2704 },
    { 0x2FF3, 2724 },
    { 0x2FE0, 2743 },
    { 0x2FCC, 2763 },
    { 0x2FB8, 2783 },
    { 0x2FA5, 2802 },
    { 0x2F91, 2822 },
    { 0x2F7E, 2841 },
    { 0x2F6A, 2861 },
    { 0x2F57, 2880 },
    { 0x2F43, 2900 },
    { 0x2F30, 2919 },
    { 0x2F1C, 2939 },
    { 0x2F09, 2958 },
    { 0x2EF5, 2978 },
    { 0x2EE2, 2997 },
    { 0x2ECE, 3017 },
    { 0x2EBB, 3036 },
    { 0x2EA8, 3055 },
    { 0x2E94, 3075 },
    { 0x2E81, 3094 },
    { 0x2E6D, 3114 },
    { 0x2E5A, 3133 },
    { 0x2E47, 3152 },
    { 0x2E33, 3172 },
    { 0x2E20, 3191 },
    { 0x2E0D, 3210 },
    { 0x2DF9, 3230 },
    { 0x2DE6, 3249 },
    { 0x2DD3, 3268 },
    { 0x2DC0, 3287 },
    { 0x2DAC, 3307 },
    { 0x2D99, 3326 },
    { 0x2D86, 3345 },
    { 0x2D73, 3364 },
    { 0x2D5F, 3384 },
    { 0x2D4C, 3403 },
    { 0x2D39, 3422 },
    { 0x2D26, 3441 },
    { 0x2D13, 3460 },
    { 0x2D00, 3479 },
    { 0x2CED, 3498 },
    { 0x2CD9, 3518 },
    { 0x2CC6, 3537 },
    { 0x2CB3, 3556 },
    { 0x2CA0, 3575 },
    { 0x2C8D, 3594 },
    { 0x2C7A, 3613 },
    { 0x2C67, 3632 },
    { 0x2C54, 3651 },
    { 0x2C41, 3670 },
    { 0x2C2F, 3688 },
    { 0x2C1C, 3707 },
    { 0x2C09, 3726 },
    { 0x2BF6, 3745 },
    { 0x2BE3, 3764 },
    { 0x2BD0, 3783 },
    { 0x2BBD, 3802 },
    { 0x2BAB, 3820 },
    { 0x2B98, 3839 },
    { 0x2B85, 3858 },
    { 0x2B73, 3876 },
    { 0x2B60, 3895 },
    { 0x2B4D, 3914 },
    { 0x2B3B, 3932 },
    { 0x2B28, 3951 },
    { 0x2B15, 3970 },
    { 0x2B03, 3988 },
    { 0x2AF0, 4007 },
    { 0x2ADE, 4025 },
    { 0x2ACB, 4044 },
    { 0x2AB9, 4062 },
    { 0x2AA6, 4081 },
    { 0x2A94, 4099 },
    { 0x2A81, 4118 },
    { 0x2A6F, 4136 },
    { 0x2A5D, 4154 },
    { 0x2A4A, 4173 },
    { 0x2A38, 4191 },
    { 0x2A26, 4209 },
    { 0x2A14, 4227 },
    { 0x2A01, 4246 },
    { 0x29EF, 4264 },
    { 0x29DD, 4282 },
    { 0x29CB, 4300 },
    { 0x29B9, 4318 },
    { 0x29A7, 4336 },
    { 0x2995, 4354 },
    { 0x2983, 4372 },
    { 0x2971, 4390 },
    { 0x295F, 4408 },
    { 0x294D, 4426 },
    { 0x293B, 4444 },
    { 0x2929, 4462 },
    { 0x2917, 4480 },
    { 0x2905, 4498 },
    { 0x28F3, 4516 },
    { 0x28E2, 4533 },
    { 0x28D0, 4551 },
    { 0x28BE, 4569 },
    { 0x28AD, 4586 },
    { 0x289B, 4604 },
    { 0x2889, 4622 },
    { 0x2878, 4639 },
    { 0x2866, 4657 },
    { 0x2855, 4674 },
    { 0x2843, 4692 },
    { 0x2832, 4709 },
    { 0x2820, 4727 },
    { 0x280F, 4744 },
    { 0x27FE, 4761 },
    { 0x27EC, 4779 },
    { 0x27DB, 4796 },
    { 0x27CA, 4813 },
    { 0x27B9, 4830 },
    { 0x27A8, 4847 },
    { 0x2796, 4865 },
    { 0x2785, 4882 },
    { 0x2774, 4899 },
    { 0x2763, 4916 },
    { 0x2752, 4933 },
    { 0x2741, 4950 },
    { 0x2730, 4967 },
    { 0x2720, 4983 },
    { 9999, 5000 },
    { 9982, 5017 },
    { 9965, 5034 },
    { 9948, 5051 },
    { 9932, 5067 },
    { 9915, 5084 },
    { 9899, 5100 },
    { 9882, 5117 },
    { 9865, 5134 },
    { 9849, 5150 },
    { 9833, 5166 },
    { 9816, 5183 },
    { 9800, 5199 },
    { 9783, 5216 },
    { 9767, 5232 },
    { 9751, 5248 },
    { 9735, 5264 },
    { 9719, 5280 },
    { 9702, 5297 },
    { 9686, 5313 },
    { 9670, 5329 },
    { 9654, 5345 },
    { 9638, 5361 },
    { 9622, 5377 },
    { 9607, 5392 },
    { 9591, 5408 },
    { 9575, 5424 },
    { 9559, 5440 },
    { 9544, 5455 },
    { 9528, 5471 },
    { 9512, 5487 },
    { 9497, 5502 },
    { 9481, 5518 },
    { 9466, 5533 },
    { 9450, 5549 },
    { 9435, 5564 },
    { 9420, 5579 },
    { 9404, 5595 },
    { 9389, 5610 },
    { 9374, 5625 },
    { 9359, 5640 },
    { 9344, 5655 },
    { 9329, 5670 },
    { 9314, 5685 },
    { 9299, 5700 },
    { 9284, 5715 },
    { 9269, 5730 },
    { 9254, 5745 },
    { 9240, 5759 },
    { 9225, 5774 },
    { 9210, 5789 },
    { 9196, 5803 },
    { 9181, 5818 },
    { 9167, 5832 },
    { 9152, 5847 },
    { 9138, 5861 },
    { 9123, 5876 },
    { 9109, 5890 },
    { 9095, 5904 },
    { 9081, 5918 },
    { 9066, 5933 },
    { 9052, 5947 },
    { 9038, 5961 },
    { 9024, 5975 },
    { 9010, 5989 },
    { 8997, 6002 },
    { 8983, 6016 },
    { 8969, 6030 },
    { 8955, 6044 },
    { 8942, 6057 },
    { 8928, 6071 },
    { 8914, 6085 },
    { 8901, 6098 },
    { 8888, 6111 },
    { 8874, 6125 },
    { 8861, 6138 },
    { 8848, 6151 },
    { 8834, 6165 },
    { 8821, 6178 },
    { 8808, 6191 },
    { 8795, 6204 },
    { 8782, 6217 },
    { 8769, 6230 },
    { 8756, 6243 },
    { 8743, 6256 },
    { 8731, 6268 },
    { 8718, 6281 },
    { 8705, 6294 },
    { 8693, 6306 },
    { 8680, 6319 },
    { 8668, 6331 },
    { 8655, 6344 },
    { 8643, 6356 },
    { 8631, 6368 },
    { 8619, 6380 },
    { 8606, 6393 },
    { 8594, 6405 },
    { 8582, 6417 },
    { 8570, 6429 },
    { 8558, 6441 },
    { 8547, 6452 },
    { 8535, 6464 },
    { 8523, 6476 },
    { 8511, 6488 },
    { 8500, 6499 },
    { 8488, 6511 },
    { 8477, 6522 },
    { 8465, 6534 },
    { 8454, 6545 },
    { 8443, 6556 },
    { 8432, 6567 },
    { 8420, 6579 },
    { 8409, 6590 },
    { 8399, 6600 },
    { 8388, 6611 },
    { 8377, 6622 },
    { 8367, 6632 },
    { 8357, 6642 },
    { 8347, 6652 },
    { 8337, 6662 },
    { 8327, 6672 },
    { 8318, 6681 },
    { 8309, 6690 },
    { 8299, 6700 },
    { 8290, 6709 },
    { 8281, 6718 },
    { 8273, 6726 },
    { 8264, 6735 },
    { 8256, 6743 },
    { 8248, 6751 },
    { 8239, 6760 },
    { 8232, 6767 },
    { 8224, 6775 },
    { 8216, 6783 },
    { 8209, 6790 },
    { 8201, 6798 },
    { 8194, 6805 },
    { 8187, 6812 },
    { 8180, 6819 },
    { 8173, 6826 },
    { 8167, 6833 },
    { 8160, 6839 },
    { 8154, 6845 },
    { 8147, 6852 },
    { 8141, 6858 },
    { 8135, 6864 },
    { 8129, 6870 },
    { 8124, 6875 },
    { 8118, 6881 },
    { 8113, 6886 },
    { 8107, 6892 },
    { 8102, 6897 },
    { 8097, 6902 },
    { 8092, 6907 },
    { 8087, 6912 },
    { 8082, 6917 },
    { 8078, 6921 },
    { 8073, 6926 },
    { 8069, 6930 },
    { 8064, 6935 },
    { 8060, 6939 },
    { 8056, 6943 },
    { 8052, 6947 },
    { 8048, 6951 },
    { 8044, 6955 },
    { 8041, 6958 },
    { 8037, 6962 },
    { 8034, 6965 },
    { 8030, 6969 },
    { 8027, 6972 },
    { 8024, 6975 },
    { 8021, 6978 },
    { 8018, 6981 },
    { 8015, 6984 },
    { 8012, 6987 },
    { 8010, 6989 },
    { 8007, 6992 },
    { 8004, 6995 },
    { 8002, 6997 },
    { 8000, 6999 },
    { 7997, 7002 },
    { 7995, 7004 },
    { 7993, 7006 },
    { 7991, 7008 },
    { 7989, 7010 },
    { 7987, 7012 },
    { 7985, 7014 },
    { 7984, 7015 },
    { 7982, 7017 },
    { 7980, 7019 },
    { 7979, 7020 },
    { 7978, 7021 },
    { 7976, 7023 },
    { 7975, 7024 },
    { 7974, 7025 },
    { 7972, 7027 },
    { 7971, 7028 },
    { 7970, 7029 },
    { 7969, 7030 },
    { 7968, 7031 },
    { 7967, 7032 },
    { 7967, 7032 },
    { 7966, 7033 },
    { 7965, 7034 },
    { 7964, 7035 },
    { 7964, 7035 },
    { 7963, 7036 },
    { 7963, 7036 },
    { 7962, 7037 },
    { 7962, 7037 },
    { 7961, 7038 },
    { 7961, 7038 },
    { 7961, 7038 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7039 },
    { 7960, 7040 },
};

static void func_actor_503500_8013223C(Task* arg0);
static void func_actor_503500_80132430(Task* arg0);

static void func_actor_503500_8013223C(Task* arg0)
{
    TmdObject*              ext;
    _Actor503500SliderWork* work;
    Enemy*                  enemy;
    GfxCoord*               coord;
    DVECTOR_XZ*             p;
    VECTOR                  pos;

    ext   = arg0->extra.tmd;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = ext->coords;
    if (work->path != 0) {
        if (work->timer < ARRAY_SIZE(D_actor_503500_80147D90)) {
            if (work->path == 1) {
                p = &D_actor_503500_80147D90[work->timer];
            } else {
                p = &D_actor_503500_80148330[work->timer];
            }
            coord->coord.t[0] = p->vx;
            coord->coord.t[2] = p->vz;
            if (!(enemy->placeKey & 0xF)) {
                if (work->timer & 1) {
                    Gp_SpawnScript18(D_actor_503500_801468A8, D_actor_503500_801468B0);
                    displaySetShakeY(-1);
                } else {
                    displaySetShakeY(0);
                }
            }
            work->timer++;
        } else {
            work->timer = 0;
            work->path  = 0;
            if (!(enemy->placeKey & 0xF)) {
                displaySetShakeY(0);
            }
        }
    } else if (work->timer > 0) {
        if (!(enemy->placeKey & 0xF)) {
            if (work->timer & 1) {
                Gp_SpawnScript18(D_actor_503500_801468A8, D_actor_503500_801468B0);
                displaySetShakeY(-1);
            } else {
                displaySetShakeY(0);
            }
        }
        work->timer--;
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        // Filled and never read: the original passes the matrix's own
        // translation instead, but the stores are still emitted.
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        worldCoordSetModelLighting(ext, coord->workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

static void func_actor_503500_80132430(Task* arg0)
{
    TmdObject*              ext;
    _Actor503500SliderWork* work;

    ext  = arg0->extra.tmd;
    work = memCalloc(sizeof(_Actor503500SliderWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    ext->flags         |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    work->freeCountdown = 0;
    func_actor_503500_801324EC(arg0);
    arg0->msgTable     = D_actor_503500_80146888;
    arg0->exitCallback = func_actor_503500_801324C4;
    arg0->state       += 1;
}

/// `Task::exitCallback` of the actor's main task, and the third entry of its
/// state table: hands the `Enemy` the spawn left in `Task::spawnArg2` back to
/// `enemyDestroy`.
static void func_actor_503500_801324C4(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

static void func_actor_503500_801324EC(Task* arg0)
{
    TmdObject*              ext;
    _Actor503500SliderWork* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

#include "../../shared/actor_messages_place_euler.inc.c"

s32 func_actor_503500_80132584(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*              obj;
    _Actor503500SliderWork* work;
    s32                     ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_503500_80132664(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor503500SliderWork* work;

    work = task->work;
    switch (msg->command) {
        case 0:
            work->path  = 0;
            work->timer = 0;
            displaySetShakeY(0);
            break;
        case 1:
            work->path                = 1;
            work->timer               = 0;
            task->extra.tmd->otOffset = 0x15;
            break;
        case 2:
            work->path                = 2;
            work->timer               = 0;
            task->extra.tmd->otOffset = 0x14;
            break;
        case 3:
            work->path  = 0;
            work->timer = 10000;
            break;
    }
    return 0;
}

/// `Task::state` handlers `func_actor_503500_8013270C` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131E24 = {
    {
        func_actor_503500_80132430,
        func_actor_503500_8013223C,
        func_actor_503500_801324C4,
    },
};

void func_actor_503500_8013270C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}
