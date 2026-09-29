#include "rooms/dryfield_night_dilapidated_house.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "dryfield_night_dilapidated_house_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room_common.h"

/// The prism corners, eight per prism: a lit ring of four, then the far ring.
extern SVECTOR D_dryfield_night_dilapidated_house_801872CC[];

extern GpGridParams D_dryfield_night_dilapidated_house_80187D44[1];

extern GpObj4C D_dryfield_night_dilapidated_house_801892A0[8];

void func_dryfield_night_dilapidated_house_8017DB20(Task*);
void func_dryfield_night_dilapidated_house_8017DCE0(Task*);

TaskDesc D_dryfield_night_dilapidated_house_8017E6F4 = { 0, 32, func_dryfield_night_dilapidated_house_8017D764, { .model = NULL } };

GpMsgEntry D_dryfield_night_dilapidated_house_8017E700[5] = {
    { 5102, func_dryfield_night_dilapidated_house_8017D8DC },
    { 5105, func_dryfield_night_dilapidated_house_8017D8D4 },
    { 5103, func_dryfield_night_dilapidated_house_8017D968 },
    { 5104, func_dryfield_night_dilapidated_house_8017D960 },
    { 0x7FFFFFFF, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[6];
    AnimationPackedRotation        words[18];
} DryfieldNightDilapidatedHousePoseBank1168;

DryfieldNightDilapidatedHousePoseBank1168 D_dryfield_night_dilapidated_house_8017E728 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017E770[46] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017E828[109] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017E9DC[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_01444_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8017EA04 = {
    D_dryfield_night_dilapidated_house_8017E828,
    D_dryfield_night_dilapidated_house_8017E9DC,
    { NULL, D_dryfield_night_dilapidated_house_8017E728, NULL, NULL, D_dryfield_night_dilapidated_house_8017E770, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[22];
    AnimationPackedRotation        words[66];
} DryfieldNightDilapidatedHousePoseBank146C;

DryfieldNightDilapidatedHousePoseBank146C D_dryfield_night_dilapidated_house_8017EA2C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017EB34[182] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017EE0C[231] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017F1A8[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_01C10_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8017F1D0 = {
    D_dryfield_night_dilapidated_house_8017EE0C,
    D_dryfield_night_dilapidated_house_8017F1A8,
    { NULL, D_dryfield_night_dilapidated_house_8017EA2C, NULL, NULL, D_dryfield_night_dilapidated_house_8017EB34, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[2];
    AnimationPackedRotation        words[6];
} DryfieldNightDilapidatedHousePoseBank1C38;

DryfieldNightDilapidatedHousePoseBank1C38 D_dryfield_night_dilapidated_house_8017F1F8 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017F210[15] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017F24C[76] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017F37C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_01DE4_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8017F3A4 = {
    D_dryfield_night_dilapidated_house_8017F24C,
    D_dryfield_night_dilapidated_house_8017F37C,
    { NULL, D_dryfield_night_dilapidated_house_8017F1F8, NULL, NULL, D_dryfield_night_dilapidated_house_8017F210, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} DryfieldNightDilapidatedHousePoseBank1E0C;

DryfieldNightDilapidatedHousePoseBank1E0C D_dryfield_night_dilapidated_house_8017F3CC = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017F420[82] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017F568[115] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017F734[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0219C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8017F75C = {
    D_dryfield_night_dilapidated_house_8017F568,
    D_dryfield_night_dilapidated_house_8017F734,
    { NULL, D_dryfield_night_dilapidated_house_8017F3CC, NULL, NULL, D_dryfield_night_dilapidated_house_8017F420, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank21C4;

DryfieldNightDilapidatedHousePoseBank21C4 D_dryfield_night_dilapidated_house_8017F784 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017F7A8[19] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017F7F4[67] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017F900[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02368_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8017F928 = {
    D_dryfield_night_dilapidated_house_8017F7F4,
    D_dryfield_night_dilapidated_house_8017F900,
    { NULL, D_dryfield_night_dilapidated_house_8017F784, NULL, NULL, D_dryfield_night_dilapidated_house_8017F7A8, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[6];
    AnimationPackedRotation        words[18];
} DryfieldNightDilapidatedHousePoseBank2390;

DryfieldNightDilapidatedHousePoseBank2390 D_dryfield_night_dilapidated_house_8017F950 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017F998[64] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017FA98[141] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017FCCC[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02734_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8017FCF4 = {
    D_dryfield_night_dilapidated_house_8017FA98,
    D_dryfield_night_dilapidated_house_8017FCCC,
    { NULL, D_dryfield_night_dilapidated_house_8017F950, NULL, NULL, D_dryfield_night_dilapidated_house_8017F998, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} DryfieldNightDilapidatedHousePoseBank275C;

DryfieldNightDilapidatedHousePoseBank275C D_dryfield_night_dilapidated_house_8017FD1C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8017FD70[54] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8017FE48[103] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8017FFE4[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02A4C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8018000C = {
    D_dryfield_night_dilapidated_house_8017FE48,
    D_dryfield_night_dilapidated_house_8017FFE4,
    { NULL, D_dryfield_night_dilapidated_house_8017FD1C, NULL, NULL, D_dryfield_night_dilapidated_house_8017FD70, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[10];
    AnimationPackedRotation        words[30];
} DryfieldNightDilapidatedHousePoseBank2A74;

DryfieldNightDilapidatedHousePoseBank2A74 D_dryfield_night_dilapidated_house_80180034 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801800AC[85] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80180200[121] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801803E4[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_02E4C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8018040C = {
    D_dryfield_night_dilapidated_house_80180200,
    D_dryfield_night_dilapidated_house_801803E4,
    { NULL, D_dryfield_night_dilapidated_house_80180034, NULL, NULL, D_dryfield_night_dilapidated_house_801800AC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} DryfieldNightDilapidatedHousePoseBank2E74;

DryfieldNightDilapidatedHousePoseBank2E74 D_dryfield_night_dilapidated_house_80180434 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80180464[17] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_801804A8[78] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801805E0[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_03048_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80180608 = {
    D_dryfield_night_dilapidated_house_801804A8,
    D_dryfield_night_dilapidated_house_801805E0,
    { NULL, D_dryfield_night_dilapidated_house_80180434, NULL, NULL, D_dryfield_night_dilapidated_house_80180464, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[8];
    AnimationPackedRotation        words[24];
} DryfieldNightDilapidatedHousePoseBank3070;

DryfieldNightDilapidatedHousePoseBank3070 D_dryfield_night_dilapidated_house_80180630 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80180690[65] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80180794[98] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8018091C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_03384_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80180944 = {
    D_dryfield_night_dilapidated_house_80180794,
    D_dryfield_night_dilapidated_house_8018091C,
    { NULL, D_dryfield_night_dilapidated_house_80180630, NULL, NULL, D_dryfield_night_dilapidated_house_80180690, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[6];
    AnimationPackedRotation        words[18];
} DryfieldNightDilapidatedHousePoseBank33AC;

DryfieldNightDilapidatedHousePoseBank33AC D_dryfield_night_dilapidated_house_8018096C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801809B4[65] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80180AB8[155] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80180D24[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0378C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80180D4C = {
    D_dryfield_night_dilapidated_house_80180AB8,
    D_dryfield_night_dilapidated_house_80180D24,
    { NULL, D_dryfield_night_dilapidated_house_8018096C, NULL, NULL, D_dryfield_night_dilapidated_house_801809B4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} DryfieldNightDilapidatedHousePoseBank37B4;

DryfieldNightDilapidatedHousePoseBank37B4 D_dryfield_night_dilapidated_house_80180D74 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80180DA4[42] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80180E4C[70] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80180F64[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_039CC_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80180F8C = {
    D_dryfield_night_dilapidated_house_80180E4C,
    D_dryfield_night_dilapidated_house_80180F64,
    { NULL, D_dryfield_night_dilapidated_house_80180D74, NULL, NULL, D_dryfield_night_dilapidated_house_80180DA4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[8];
    AnimationPackedRotation        words[24];
} DryfieldNightDilapidatedHousePoseBank39F4;

DryfieldNightDilapidatedHousePoseBank39F4 D_dryfield_night_dilapidated_house_80180FB4 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80181014[84] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80181164[117] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80181338[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_03DA0_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80181360 = {
    D_dryfield_night_dilapidated_house_80181164,
    D_dryfield_night_dilapidated_house_80181338,
    { NULL, D_dryfield_night_dilapidated_house_80180FB4, NULL, NULL, D_dryfield_night_dilapidated_house_80181014, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} DryfieldNightDilapidatedHousePoseBank3DC8;

DryfieldNightDilapidatedHousePoseBank3DC8 D_dryfield_night_dilapidated_house_80181388 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801813DC[74] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80181504[104] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801816A4[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0410C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801816CC = {
    D_dryfield_night_dilapidated_house_80181504,
    D_dryfield_night_dilapidated_house_801816A4,
    { NULL, D_dryfield_night_dilapidated_house_80181388, NULL, NULL, D_dryfield_night_dilapidated_house_801813DC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[5];
    AnimationPackedRotation        words[15];
} DryfieldNightDilapidatedHousePoseBank4134;

DryfieldNightDilapidatedHousePoseBank4134 D_dryfield_night_dilapidated_house_801816F4 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80181730[80] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80181870[162] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80181AF8[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_04560_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80181B20 = {
    D_dryfield_night_dilapidated_house_80181870,
    D_dryfield_night_dilapidated_house_80181AF8,
    { NULL, D_dryfield_night_dilapidated_house_801816F4, NULL, NULL, D_dryfield_night_dilapidated_house_80181730, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} DryfieldNightDilapidatedHousePoseBank4588;

DryfieldNightDilapidatedHousePoseBank4588 D_dryfield_night_dilapidated_house_80181B48 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80181B78[51] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80181C44[82] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80181D8C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_047F4_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80181DB4 = {
    D_dryfield_night_dilapidated_house_80181C44,
    D_dryfield_night_dilapidated_house_80181D8C,
    { NULL, D_dryfield_night_dilapidated_house_80181B48, NULL, NULL, D_dryfield_night_dilapidated_house_80181B78, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[7];
    AnimationPackedRotation        words[21];
} DryfieldNightDilapidatedHousePoseBank481C;

DryfieldNightDilapidatedHousePoseBank481C D_dryfield_night_dilapidated_house_80181DDC = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80181E30[56] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80181F10[106] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801820B8[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_04B20_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801820E0 = {
    D_dryfield_night_dilapidated_house_80181F10,
    D_dryfield_night_dilapidated_house_801820B8,
    { NULL, D_dryfield_night_dilapidated_house_80181DDC, NULL, NULL, D_dryfield_night_dilapidated_house_80181E30, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank4B48;

DryfieldNightDilapidatedHousePoseBank4B48 D_dryfield_night_dilapidated_house_80182108 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8018212C[81] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80182270[128] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80182470[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_04ED8_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80182498 = {
    D_dryfield_night_dilapidated_house_80182270,
    D_dryfield_night_dilapidated_house_80182470,
    { NULL, D_dryfield_night_dilapidated_house_80182108, NULL, NULL, D_dryfield_night_dilapidated_house_8018212C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[5];
    AnimationPackedRotation        words[15];
} DryfieldNightDilapidatedHousePoseBank4F00;

DryfieldNightDilapidatedHousePoseBank4F00 D_dryfield_night_dilapidated_house_801824C0 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801824FC[58] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_801825E4[120] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801827C4[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0522C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801827EC = {
    D_dryfield_night_dilapidated_house_801825E4,
    D_dryfield_night_dilapidated_house_801827C4,
    { NULL, D_dryfield_night_dilapidated_house_801824C0, NULL, NULL, D_dryfield_night_dilapidated_house_801824FC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank5254;

DryfieldNightDilapidatedHousePoseBank5254 D_dryfield_night_dilapidated_house_80182814 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80182838[34] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_801828C0[63] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801829BC[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_05424_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801829E4 = {
    D_dryfield_night_dilapidated_house_801828C0,
    D_dryfield_night_dilapidated_house_801829BC,
    { NULL, D_dryfield_night_dilapidated_house_80182814, NULL, NULL, D_dryfield_night_dilapidated_house_80182838, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank544C;

DryfieldNightDilapidatedHousePoseBank544C D_dryfield_night_dilapidated_house_80182A0C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80182A30[27] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80182A9C[57] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80182B80[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_055E8_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80182BA8 = {
    D_dryfield_night_dilapidated_house_80182A9C,
    D_dryfield_night_dilapidated_house_80182B80,
    { NULL, D_dryfield_night_dilapidated_house_80182A0C, NULL, NULL, D_dryfield_night_dilapidated_house_80182A30, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} DryfieldNightDilapidatedHousePoseBank5610;

DryfieldNightDilapidatedHousePoseBank5610 D_dryfield_night_dilapidated_house_80182BD0 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80182C00[40] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80182CA0[104] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80182E40[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_058A8_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80182E68 = {
    D_dryfield_night_dilapidated_house_80182CA0,
    D_dryfield_night_dilapidated_house_80182E40,
    { NULL, D_dryfield_night_dilapidated_house_80182BD0, NULL, NULL, D_dryfield_night_dilapidated_house_80182C00, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank58D0;

DryfieldNightDilapidatedHousePoseBank58D0 D_dryfield_night_dilapidated_house_80182E90 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80182EB4[27] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80182F20[57] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80183004[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_05A6C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8018302C = {
    D_dryfield_night_dilapidated_house_80182F20,
    D_dryfield_night_dilapidated_house_80183004,
    { NULL, D_dryfield_night_dilapidated_house_80182E90, NULL, NULL, D_dryfield_night_dilapidated_house_80182EB4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank5A94;

DryfieldNightDilapidatedHousePoseBank5A94 D_dryfield_night_dilapidated_house_80183054 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80183078[48] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80183138[123] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80183324[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_05D8C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_8018334C = {
    D_dryfield_night_dilapidated_house_80183138,
    D_dryfield_night_dilapidated_house_80183324,
    { NULL, D_dryfield_night_dilapidated_house_80183054, NULL, NULL, D_dryfield_night_dilapidated_house_80183078, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[2];
    AnimationPackedRotation        words[6];
} DryfieldNightDilapidatedHousePoseBank5DB4;

DryfieldNightDilapidatedHousePoseBank5DB4 D_dryfield_night_dilapidated_house_80183374 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8018338C[32] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8018340C[101] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801835A0[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_06008_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801835C8 = {
    D_dryfield_night_dilapidated_house_8018340C,
    D_dryfield_night_dilapidated_house_801835A0,
    { NULL, D_dryfield_night_dilapidated_house_80183374, NULL, NULL, D_dryfield_night_dilapidated_house_8018338C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[21];
    AnimationPackedRotation        words[63];
} DryfieldNightDilapidatedHousePoseBank6030;

DryfieldNightDilapidatedHousePoseBank6030 D_dryfield_night_dilapidated_house_801835F0 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801836EC[156] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8018395C[246] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80183D34[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0679C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80183D5C = {
    D_dryfield_night_dilapidated_house_8018395C,
    D_dryfield_night_dilapidated_house_80183D34,
    { NULL, D_dryfield_night_dilapidated_house_801835F0, NULL, NULL, D_dryfield_night_dilapidated_house_801836EC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[2];
    AnimationPackedRotation        words[6];
} DryfieldNightDilapidatedHousePoseBank67C4;

DryfieldNightDilapidatedHousePoseBank67C4 D_dryfield_night_dilapidated_house_80183D84 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80183D9C[135] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80183FB8[188] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801842A8[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_06D10_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801842D0 = {
    D_dryfield_night_dilapidated_house_80183FB8,
    D_dryfield_night_dilapidated_house_801842A8,
    { NULL, D_dryfield_night_dilapidated_house_80183D84, NULL, NULL, D_dryfield_night_dilapidated_house_80183D9C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank6D38;

DryfieldNightDilapidatedHousePoseBank6D38 D_dryfield_night_dilapidated_house_801842F8 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8018431C[29] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80184390[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80184480[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_06EE8_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801844A8 = {
    D_dryfield_night_dilapidated_house_80184390,
    D_dryfield_night_dilapidated_house_80184480,
    { NULL, D_dryfield_night_dilapidated_house_801842F8, NULL, NULL, D_dryfield_night_dilapidated_house_8018431C, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank6F10;

DryfieldNightDilapidatedHousePoseBank6F10 D_dryfield_night_dilapidated_house_801844D0 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801844F4[80] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80184634[154] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8018489C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07304_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_801848C4 = {
    D_dryfield_night_dilapidated_house_80184634,
    D_dryfield_night_dilapidated_house_8018489C,
    { NULL, D_dryfield_night_dilapidated_house_801844D0, NULL, NULL, D_dryfield_night_dilapidated_house_801844F4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[2];
    AnimationPackedRotation        words[6];
} DryfieldNightDilapidatedHousePoseBank732C;

DryfieldNightDilapidatedHousePoseBank732C D_dryfield_night_dilapidated_house_801848EC = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80184904[25] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80184968[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80184A58[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_074C0_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80184A80 = {
    D_dryfield_night_dilapidated_house_80184968,
    D_dryfield_night_dilapidated_house_80184A58,
    { NULL, D_dryfield_night_dilapidated_house_801848EC, NULL, NULL, D_dryfield_night_dilapidated_house_80184904, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank74E8;

DryfieldNightDilapidatedHousePoseBank74E8 D_dryfield_night_dilapidated_house_80184AA8 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80184ACC[33] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80184B50[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80184C40[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_076A8_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80184C68 = {
    D_dryfield_night_dilapidated_house_80184B50,
    D_dryfield_night_dilapidated_house_80184C40,
    { NULL, D_dryfield_night_dilapidated_house_80184AA8, NULL, NULL, D_dryfield_night_dilapidated_house_80184ACC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank76D0;

DryfieldNightDilapidatedHousePoseBank76D0 D_dryfield_night_dilapidated_house_80184C90 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80184CB4[39] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80184D50[107] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80184EFC[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07964_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80184F24 = {
    D_dryfield_night_dilapidated_house_80184D50,
    D_dryfield_night_dilapidated_house_80184EFC,
    { NULL, D_dryfield_night_dilapidated_house_80184C90, NULL, NULL, D_dryfield_night_dilapidated_house_80184CB4, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank798C;

DryfieldNightDilapidatedHousePoseBank798C D_dryfield_night_dilapidated_house_80184F4C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80184F70[31] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80184FEC[60] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_records.inc"
};

u16 D_dryfield_night_dilapidated_house_801850DC[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07B44_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80185104 = {
    D_dryfield_night_dilapidated_house_80184FEC,
    D_dryfield_night_dilapidated_house_801850DC,
    { NULL, D_dryfield_night_dilapidated_house_80184F4C, NULL, NULL, D_dryfield_night_dilapidated_house_80184F70, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[5];
    AnimationPackedRotation        words[15];
} DryfieldNightDilapidatedHousePoseBank7B6C;

DryfieldNightDilapidatedHousePoseBank7B6C D_dryfield_night_dilapidated_house_8018512C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80185168[41] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8018520C[76] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8018533C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07DA4_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80185364 = {
    D_dryfield_night_dilapidated_house_8018520C,
    D_dryfield_night_dilapidated_house_8018533C,
    { NULL, D_dryfield_night_dilapidated_house_8018512C, NULL, NULL, D_dryfield_night_dilapidated_house_80185168, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[3];
    AnimationPackedRotation        words[9];
} DryfieldNightDilapidatedHousePoseBank7DCC;

DryfieldNightDilapidatedHousePoseBank7DCC D_dryfield_night_dilapidated_house_8018538C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801853B0[21] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80185404[87] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80185560[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_07FC8_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80185588 = {
    D_dryfield_night_dilapidated_house_80185404,
    D_dryfield_night_dilapidated_house_80185560,
    { NULL, D_dryfield_night_dilapidated_house_8018538C, NULL, NULL, D_dryfield_night_dilapidated_house_801853B0, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[5];
    AnimationPackedRotation        words[15];
} DryfieldNightDilapidatedHousePoseBank7FF0;

DryfieldNightDilapidatedHousePoseBank7FF0 D_dryfield_night_dilapidated_house_801855B0 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_801855EC[52] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_801856BC[85] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80185810[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_08278_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80185838 = {
    D_dryfield_night_dilapidated_house_801856BC,
    D_dryfield_night_dilapidated_house_80185810,
    { NULL, D_dryfield_night_dilapidated_house_801855B0, NULL, NULL, D_dryfield_night_dilapidated_house_801855EC, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[2];
    AnimationPackedRotation        words[6];
} DryfieldNightDilapidatedHousePoseBank82A0;

DryfieldNightDilapidatedHousePoseBank82A0 D_dryfield_night_dilapidated_house_80185860 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80185878[87] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_801859D4[136] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_records.inc"
};

u16 D_dryfield_night_dilapidated_house_80185BF4[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_0865C_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80185C1C = {
    D_dryfield_night_dilapidated_house_801859D4,
    D_dryfield_night_dilapidated_house_80185BF4,
    { NULL, D_dryfield_night_dilapidated_house_80185860, NULL, NULL, D_dryfield_night_dilapidated_house_80185878, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} DryfieldNightDilapidatedHousePoseBank8684;

DryfieldNightDilapidatedHousePoseBank8684 D_dryfield_night_dilapidated_house_80185C44 = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_80185C74[147] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_80185EC0[211] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8018620C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_08C74_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80186234 = {
    D_dryfield_night_dilapidated_house_80185EC0,
    D_dryfield_night_dilapidated_house_8018620C,
    { NULL, D_dryfield_night_dilapidated_house_80185C44, NULL, NULL, D_dryfield_night_dilapidated_house_80185C74, NULL, NULL, NULL },
};

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    AnimationPackedPose poses[4];
    AnimationPackedRotation        words[12];
} DryfieldNightDilapidatedHousePoseBank8C9C;

DryfieldNightDilapidatedHousePoseBank8C9C D_dryfield_night_dilapidated_house_8018625C = { .poses = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_dilapidated_house_8018628C[36] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_bank4.inc"
};

GpAnimRec D_dryfield_night_dilapidated_house_8018631C[84] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_records.inc"
};

u16 D_dryfield_night_dilapidated_house_8018646C[20] = {
#include "assets/dryfield_night_dilapidated_house_animation_08ED4_indices.inc"
};

GpAnimSet D_dryfield_night_dilapidated_house_80186494 = {
    D_dryfield_night_dilapidated_house_8018631C,
    D_dryfield_night_dilapidated_house_8018646C,
    { NULL, D_dryfield_night_dilapidated_house_8018625C, NULL, NULL, D_dryfield_night_dilapidated_house_8018628C, NULL, NULL, NULL },
};

GpAnimSet* D_dryfield_night_dilapidated_house_801864BC[25] = {
    NULL,
    &D_dryfield_night_dilapidated_house_8017EA04,
    &D_dryfield_night_dilapidated_house_8017F1D0,
    &D_dryfield_night_dilapidated_house_8017F3A4,
    &D_dryfield_night_dilapidated_house_8017F75C,
    &D_dryfield_night_dilapidated_house_8017F928,
    &D_dryfield_night_dilapidated_house_8017FCF4,
    &D_dryfield_night_dilapidated_house_8018000C,
    &D_dryfield_night_dilapidated_house_8018040C,
    &D_dryfield_night_dilapidated_house_80180608,
    &D_dryfield_night_dilapidated_house_80180944,
    &D_dryfield_night_dilapidated_house_80180D4C,
    &D_dryfield_night_dilapidated_house_80180F8C,
    &D_dryfield_night_dilapidated_house_80181360,
    &D_dryfield_night_dilapidated_house_801816CC,
    &D_dryfield_night_dilapidated_house_80181B20,
    &D_dryfield_night_dilapidated_house_80181DB4,
    &D_dryfield_night_dilapidated_house_801820E0,
    &D_dryfield_night_dilapidated_house_80182498,
    &D_dryfield_night_dilapidated_house_801827EC,
    &D_dryfield_night_dilapidated_house_801829E4,
    &D_dryfield_night_dilapidated_house_80182BA8,
    &D_dryfield_night_dilapidated_house_80182E68,
    &D_dryfield_night_dilapidated_house_8018302C,
    &D_dryfield_night_dilapidated_house_8018334C,
};

GpCopyArg D_dryfield_night_dilapidated_house_80186520 = { { .sets = D_dryfield_night_dilapidated_house_801864BC }, 25 };

GpAnimArg D_dryfield_night_dilapidated_house_80186528 = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_8018653C = { { .index = 1 }, 48, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186550 = { { .index = 1 }, 9, 0, 0, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186564[2] = {
    { { .index = 1 }, 50, 1, 20, 1 },
    { { .index = 1 }, 51, 1, 20, 1 },
};

GpAnimArg D_dryfield_night_dilapidated_house_8018658C = { { .index = 1 }, 52, 1, 4, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801865A0 = { { .index = 1 }, 53, 1, 4, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801865B4 = { { .index = 1 }, 54, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801865C8 = { { .index = 1 }, 55, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801865DC = { { .index = 1 }, 56, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801865F0 = { { .index = 1 }, 57, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186604[6] = {
    { { .index = 1 }, 58, 1, 20, 1 },
    { { .index = 1 }, 59, 1, 20, 1 },
    { { .index = 1 }, 60, 1, 20, 1 },
    { { .index = 1 }, 61, 1, 20, 1 },
    { { .index = 1 }, 62, 1, 20, 1 },
    { { .index = 1 }, 63, 1, 20, 1 },
};

GpAnimArg D_dryfield_night_dilapidated_house_8018667C = { { .index = 1 }, 64, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186690 = { { .index = 1 }, 65, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801866A4[5] = {
    { { .index = 1 }, 66, 0, 0, 1 },
    { { .index = 1 }, 67, 1, 20, 1 },
    { { .index = 1 }, 68, 1, 20, 1 },
    { { .index = 1 }, 69, 1, 20, 1 },
    { { .index = 1 }, 70, 1, 20, 1 },
};

GpAnimArg D_dryfield_night_dilapidated_house_80186708 = { { .index = 1 }, 71, 1, 10, 1 };

GpXformArg D_dryfield_night_dilapidated_house_8018671C = { { 2900, 0, 0, 0 }, { 0, -1024, 0, 0 } };

GpAnimSet* D_dryfield_night_dilapidated_house_80186734[16] = {
    NULL,
    &D_dryfield_night_dilapidated_house_801835C8,
    &D_dryfield_night_dilapidated_house_80185364,
    &D_dryfield_night_dilapidated_house_80185588,
    &D_dryfield_night_dilapidated_house_80185838,
    &D_dryfield_night_dilapidated_house_80185C1C,
    &D_dryfield_night_dilapidated_house_80186234,
    &D_dryfield_night_dilapidated_house_801844A8,
    &D_dryfield_night_dilapidated_house_801848C4,
    &D_dryfield_night_dilapidated_house_80184A80,
    &D_dryfield_night_dilapidated_house_80184C68,
    &D_dryfield_night_dilapidated_house_80184F24,
    &D_dryfield_night_dilapidated_house_80185104,
    &D_dryfield_night_dilapidated_house_801842D0,
    &D_dryfield_night_dilapidated_house_80183D5C,
    &D_dryfield_night_dilapidated_house_80186494,
};

GpCopyArg D_dryfield_night_dilapidated_house_80186774 = { { .sets = D_dryfield_night_dilapidated_house_80186734 }, 16 };

GpAnimArg D_dryfield_night_dilapidated_house_8018677C = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186790 = { { .index = 1 }, 48, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801867A4 = { { .index = 1 }, 49, 0, 0, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801867B8 = { { .index = 1 }, 50, 0, 0, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801867CC = { { .index = 1 }, 51, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801867E0 = { { .index = 1 }, 52, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801867F4 = { { .index = 1 }, 53, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186808 = { { .index = 1 }, 54, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_8018681C = { { .index = 1 }, 55, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186830 = { { .index = 1 }, 56, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186844 = { { .index = 1 }, 57, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186858 = { { .index = 1 }, 58, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_8018686C = { { .index = 1 }, 59, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186880 = { { .index = 1 }, 60, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_80186894 = { { .index = 1 }, 61, 1, 20, 1 };

GpAnimArg D_dryfield_night_dilapidated_house_801868A8 = { { .index = 1 }, 62, 1, 10, 1 };

GpXformArg D_dryfield_night_dilapidated_house_801868BC = { { 400, 0, 0, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_dryfield_night_dilapidated_house_801868D4 = { { 1540, 0, 0, 0 }, { 0, 1024, 0, 0 } };

GpOverlayIds D_dryfield_night_dilapidated_house_801868EC = { 3, 50, 11 };

GpEvsCmd D_dryfield_night_dilapidated_house_801868F4[88] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DAF0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_dryfield_night_dilapidated_house_801868EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DA70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DA90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_dilapidated_house_80186520 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_night_dilapidated_house_80186774 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_dilapidated_house_8018671C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_dilapidated_house_801868BC }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186550 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801867B8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801867CC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_dryfield_night_dilapidated_house_801868D4 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186880 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018658C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801865A0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801867E0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_dilapidated_house_801868D4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801865C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186880 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801865F0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186708 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018667C }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186790 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_801868A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186808 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018681C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186830 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186708 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186844 }, { .value = 0 } },
    { 4, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186858 }, { .value = 0 } },
    { 4, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018686C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186690 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_8018653C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_dilapidated_house_80186880 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DAB0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_dilapidated_house_80187134[16] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_dilapidated_house_8018671C }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_dilapidated_house_801868D4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_night_dilapidated_house_8017DAD0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 48, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_night_dilapidated_house_801872B4[2] = {
    { 0, 192, func_dryfield_night_dilapidated_house_8017DCE0, { .model = NULL } },
    { 0, 192, func_dryfield_night_dilapidated_house_8017DB20, { .model = NULL } },
};

SVECTOR D_dryfield_night_dilapidated_house_801872CC[24] = {
    { -5500, -2250, -3030, 0 },
    { -4500, -2250, -3030, 0 },
    { -4500, -1030, -3030, 0 },
    { -5500, -1030, -3030, 0 },
    { -5780, 0, -180, 0 },
    { -5000, 0, -180, 0 },
    { -4770, 0, -1660, 0 },
    { -5660, 0, -1660, 0 },
    { -3000, -2250, -3030, 0 },
    { -2000, -2250, -3030, 0 },
    { -2000, -1030, -3030, 0 },
    { -3000, -1030, -3030, 0 },
    { -3370, 0, -180, 0 },
    { -2500, 0, -180, 0 },
    { -2770, 0, -1660, 0 },
    { -3140, 0, -1660, 0 },
    { -500, -2250, -3030, 0 },
    { 500, -2250, -3030, 0 },
    { 500, -1030, -3030, 0 },
    { -500, -1030, -3030, 0 },
    { -870, 0, -180, 0 },
    { 0, 0, -180, 0 },
    { 230, 0, -1660, 0 },
    { -650, 0, -1660, 0 },
};

GpRoomCoordRec D_dryfield_night_dilapidated_house_8018738C[1] = {
    { D_dryfield_night_dilapidated_house_80189B60, D_dryfield_night_dilapidated_house_8018A054 },
};

GpRoomObjRec D_dryfield_night_dilapidated_house_80187394[1] = {
    { D_dryfield_night_dilapidated_house_80187D44, D_dryfield_night_dilapidated_house_801892A0, D_dryfield_night_dilapidated_house_80189B78, D_dryfield_night_dilapidated_house_80189F08 },
};

u8* D_dryfield_night_dilapidated_house_801873A4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_dilapidated_house_801873A8[1] = {
    { { .bytes = { 11, 0 } } },
};

GpWarpRec D_dryfield_night_dilapidated_house_801873AC[3] = {
    { { .words = { 0, 1596, 0, -2540 } }, { 0, 0, 0, 0 }, { .words = { 0, 2296, 0, -1578 } }, { 0, 0, 0, 0 }, 0x53090002, 0x53090001, 0, 6, 0, 467 },
    { { .words = { 1024, -5403, 2, -400 } }, { 0, 0, 0, 0 }, { .words = { 2048, -5184, 2, 200 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 0, 1596, 0, -2540 } }, { 0, 0, 0, 0 }, { .words = { 0, 2296, 0, -1578 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 0, 0 },
};

SVECTOR D_dryfield_night_dilapidated_house_80187454[10] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_normals.inc"
};

SVECTOR D_dryfield_night_dilapidated_house_801874A4[116] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_verts.inc"
};

GpGridFace D_dryfield_night_dilapidated_house_80187844[70] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_faces.inc"
};

s16 D_dryfield_night_dilapidated_house_80187B8C[208] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_dilapidated_house_80187B8C[i])
s16* D_dryfield_night_dilapidated_house_80187D2C[6] = {
#include "assets/dryfield_night_dilapidated_house_collision_0A784_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_dilapidated_house_80187D44[1] = {
    { NULL, D_dryfield_night_dilapidated_house_80187454, D_dryfield_night_dilapidated_house_801874A4, D_dryfield_night_dilapidated_house_80187844, D_dryfield_night_dilapidated_house_80187D2C, 6000, 3200, 3, 2, 4000, 70 },
};

GpViewRec D_dryfield_night_dilapidated_house_80187D68[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 900, 0x4E20, 0 } }, 490 },
    { { { { 3872, 0, -1335 }, { -1200, 1793, -3481 }, { 584, 3682, 1695 } }, { 5380, 4300, 2840 } }, 207 },
    { { { { -2538, 0, 3214 }, { 1329, 3729, 1049 }, { -2926, 1694, -2310 } }, { 200, 2400, -200 } }, 257 },
    { { { { -1264, 0, 3896 }, { 486, 4063, 158 }, { -3865, 511, -1254 } }, { -3950, 2000, -1850 } }, 230 },
    { { { { -1264, 0, -3896 }, { -486, 4063, 158 }, { 3865, 511, -1254 } }, { 3950, 2000, -1850 } }, 230 },
    { { { { -3842, 0, -1419 }, { -415, 3916, 1123 }, { 1357, 1198, -3674 } }, { -600, 2000, -600 } }, 257 },
    { { { { -2678, 0, 3098 }, { -221, 4085, -191 }, { -3090, -292, -2671 } }, { -5725, 703, -3247 } }, 447 },
    { { { { -1861, 0, 3648 }, { 0, 4096, 0 }, { -3648, 0, -1861 } }, { -4445, 1450, -1015 } }, 447 },
    { { { { -2246, 0, -3425 }, { 0, 4096, 0 }, { 3425, 0, -2246 } }, { -1229, 1400, -1022 } }, 447 },
    { { { { -3166, 0, -2598 }, { -493, 4021, 600 }, { 2550, 777, -3108 } }, { -720, 1805, -1720 } }, 447 },
    { { { { -2242, 0, 3427 }, { 0, 4096, 0 }, { -3427, 0, -2242 } }, { -3395, 1400, -1085 } }, 447 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80187EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_80187F04[71] = {
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -48, 641, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 623, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 617, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -56, 596, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, -40, 581, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -32, 520, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -16, 500, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, -8, 471, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 8, 458, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 88, 337, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -120, 1066, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -120, 1116, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -120, 1192, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, -120, 1185, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 0, -120, 1100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 8, -120, 905, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 8, -64, 1048, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -48, 1000, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 16, -120, 841, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 24, -120, 771, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 24, -40, 972, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 32, -72, 794, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -120, 680, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 40, -120, 625, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, 40, 750, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 64, 32, 750, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 32, 750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -120, 589, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 56, -120, 541, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 56, -64, 636, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 617, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -120, 514, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -120, 488, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -48, 599, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, -120, 451, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, -40, 556, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 80, 24, 750, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 96, 24, 466, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -40, 468, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -120, 406, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 112, -120, 369, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, -40, 418, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, 24, 483, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 128, 24, 432, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, -40, 379, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, -120, 338, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -120, 312, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -40, 347, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 24, 387, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 24, 1125, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 48, 56, 1125, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -56, 649, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, -32, 841, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -64, 762, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, 16, 1125, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -24, 889, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, -24, 885, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 32, 16, 1107, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 375, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 375, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -32, 64, 375, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -16, 64, 375, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, 56, 375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, 56, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, 48, 375, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 48, 40, 375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 32, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 24, 375, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, 24, 375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 32, 375, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80188490[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 49, 0, 0, { 2, 0 } },
    { 58, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_801884B8[62] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -48, 1227, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -32, 1299, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, 0, 1163, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, -120, 1159, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -8, -120, 1133, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -80, 1227, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -32, 1313, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 8, -32, 1268, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -80, 1164, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 8, -120, 1083, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1055, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 1017, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -120, 983, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, 8, 1500, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 8, 1500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -120, 1125, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 72, -72, 1125, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -16, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 897, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 88, -64, 823, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -8, 821, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -120, 766, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, -64, 792, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -8, 876, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -120, 743, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 120, -56, 882, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 0, 766, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, -120, 734, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 136, -56, 659, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 136, 0, 741, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 914, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -48, 700, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -120, 716, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 16, 1500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -8, 1390, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -72, 1015, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -72, 1015, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -16, 1127, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -16, 1102, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, -120, 1426, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, -120, 941, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 854, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -160, 16, 703, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, 8, 738, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -128, 8, 776, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -96, 16, 856, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 835, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -104, 8, 830, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -112, 8, 806, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 893, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 48, 862, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 8, 936, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 80, -48, 845, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -88, 800, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 858, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 24, 930, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -88, 773, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -24, 834, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, 32, 912, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -88, 746, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -24, 876, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 32, 837, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80188990[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 41, 0, 0, { 1, 0 } },
    { 41, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_801889B0[45] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -64, 1503, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, -32, 2189, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 2230, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -32, 1391, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -128, -32, 1416, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -120, -32, 1523, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 8, 1458, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, 0, 1523, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 8, 1601, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 8, 1665, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 32, 1698, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -96, 1376, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -48, 2240, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -8, 2262, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -24, 2271, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -48, 2180, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -96, 1348, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -96, 1361, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, -32, 2155, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -96, 1710, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -32, 1987, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -104, 1472, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -48, 1844, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 8, 1980, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 8, 1952, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, -48, 2017, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, -104, 1441, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, -104, 1411, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, -48, 1972, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 32, 8, 2008, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 1927, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -48, 1932, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -104, 1263, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -104, 1263, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -48, 1892, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 8, 1874, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, -112, 1162, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -48, 1834, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 8, 1824, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 8, 1587, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -48, 1716, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -112, 1162, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, -112, 1162, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -48, 1798, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 8, 1628, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80188D34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 45, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_80188D4C[7] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 1429, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 1397, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 0, 1475, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 8, 1392, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1313, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 8, 1313, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 1126, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80188DD8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_80188DF0[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 837, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 828, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 88, 8, 834, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 96, 8, 834, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 104, -16, 804, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 112, -8, 861, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 120, -8, 861, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 0, 834, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, 8, 808, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 144, 0, 834, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 152, 0, 735, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80188ECC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_80188EE4[9] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -64, 2714, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -72, 2500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, 0, 2475, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 64, 0, 2700, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 64, -64, 2533, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, -72, 2433, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 128, 0, 2333, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -80, 2426, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -8, 2279, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80188F98[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_80188FB0[15] = {
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 8, -80, 2329, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 8, 0, 2179, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 24, -80, 2164, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 24, 0, 2269, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -80, 2216, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, -88, 2185, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 72, -8, 2149, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, -88, 2137, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 88, -8, 2149, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 104, -88, 2101, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 104, -8, 1964, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 120, -88, 1966, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -8, 1936, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 136, -88, 2027, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 136, -8, 1896, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_801890DC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_dilapidated_house_801890F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80189104[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_dilapidated_house_80189114[12] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -88, 2089, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 32, 0, 2087, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, -96, 1918, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, -8, 2035, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -96, 1976, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -96, 1936, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 104, -96, 1906, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 104, -8, 1898, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 120, -96, 1868, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 120, -8, 1861, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, -104, 1763, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, -8, 1734, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_dilapidated_house_80189204[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_dilapidated_house_8018921C[11] = {
    { { .empty = D_dryfield_night_dilapidated_house_80187EF4 }, D_dryfield_night_dilapidated_house_80187EF4, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80187F04 }, D_dryfield_night_dilapidated_house_80188490, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_801884B8 }, D_dryfield_night_dilapidated_house_80188990, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_801889B0 }, D_dryfield_night_dilapidated_house_80188D34, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188D4C }, D_dryfield_night_dilapidated_house_80188DD8, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188DF0 }, D_dryfield_night_dilapidated_house_80188ECC, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188EE4 }, D_dryfield_night_dilapidated_house_80188F98, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80188FB0 }, D_dryfield_night_dilapidated_house_801890DC, NULL },
    { { .empty = D_dryfield_night_dilapidated_house_801890F4 }, D_dryfield_night_dilapidated_house_801890F4, NULL },
    { { .empty = D_dryfield_night_dilapidated_house_80189104 }, D_dryfield_night_dilapidated_house_80189104, NULL },
    { { .elements = D_dryfield_night_dilapidated_house_80189114 }, D_dryfield_night_dilapidated_house_80189204, NULL },
};

GpObj4C D_dryfield_night_dilapidated_house_801892A0[8] = {
    { NULL, NULL, NULL, { -4160, -1696, -1792, 0 }, { { 0, -2112, -1024, 0 }, { 0, -2112, 1024, 0 }, { 0, 2112, -1024, 0 }, { 0, 2112, 1024, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -4064, -1376, -1824, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1024, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1024, 0 } }, { -4117, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -1937, -1376, -1937, 0 }, { { -2182, -2400, 2208, 0 }, { 2183, -2400, -2207, 0 }, { -2182, 2400, 2208, 0 }, { 2183, 2400, -2207, 0 } }, { -2920, 0, -2887, 0 }, { 0, 0, 4096, 0 }, 3916, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -2017, -1360, -2017, 0 }, { { 2183, -2384, -2207, 0 }, { -2182, -2384, 2208, 0 }, { 2183, 2384, -2207, 0 }, { -2182, 2384, 2208, 0 } }, { 2913, 0, 2880, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 220, -1392, 318, 0 }, { { -30, -2416, -3113, 0 }, { 4, -2416, 3091, 0 }, { -30, 2416, -3113, 0 }, { 4, 2416, 3091, 0 } }, { 4098, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3932, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 2974, -1312, -1986, 0 }, { { -2235, -2336, -137, 0 }, { 2226, -2336, 127, 0 }, { -2235, 2336, -137, 0 }, { 2226, 2336, 127, 0 } }, { 241, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 318, -1360, 352, 0 }, { { 13, -2384, 3100, 0 }, { -23, -2384, -3109, 0 }, { 13, 2384, 3100, 0 }, { -23, 2384, -3109, 0 } }, { -4098, 0, 23, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 2975, -1472, -2048, 0 }, { { 2226, -2496, 127, 0 }, { -2235, -2496, -137, 0 }, { 2226, 2496, 127, 0 }, { -2235, 2496, -137, 0 } }, { -243, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 5, 6, 129, 0 },
};

GpPointLight D_dryfield_night_dilapidated_house_80189500[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -1500, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2000, 3549 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2256, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2256, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2256, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2000, 6400 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2000, 3003 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1232, 1232, { 0, 0 } }, 2000, 3000 },
};

DryfieldNightDilapidatedHouseSpotLightStorage D_dryfield_night_dilapidated_house_80189800 = {
    {
        { { { .coord = { 0, { { { -4096, 0, 0 }, { 0, -2902, 2901 }, { 0, 2901, 2901 } }, { -5000, -2500, -4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } }, { 0, 2896, 2896, 0 }, 100, 3000, 625 },
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
        0x08,
        0x00,
        0x00,
        0x00,
        0xA4,
        0x50,
        0x19,
        0x80,
        0x01,
        0x00,
        0x00,
        0x00,
        0xA4,
        0x53,
        0x00,
        0x00,
        0x2D,
        0xFC,
        0x80,
        0x10,
        0xD8,
        0xFF,
        0x00,
        0x00,
        0x55,
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
        0xBE,
        0xE7,
        0x30,
        0xF3,
        0xDE,
        0x15,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x07,
        0x00,
        0x00,
        0x00,
        0xD4,
        0x03,
        0x00,
        0x00,
        0xF9,
        0xFF,
        0x00,
        0x00,
        0x2D,
        0xFC,
        0x00,
        0x00,
        0x07,
        0x00,
        0x00,
        0x00,
        0xD4,
        0x03,
        0xD0,
        0x10,
        0xF9,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xEB,
        0xEF,
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
        0x34,
        0x11,
        0x00,
        0x00,
        0x06,
        0x05,
        0x00,
        0x00,
        0xE0,
        0x54,
        0x19,
        0x80,
        0x48,
        0x54,
        0x19,
        0x80,
        0xF8,
        0x30,
        0x07,
        0x80,
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
        0xC0,
        0x03,
        0xE0,
        0x11,
        0x3A,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x51,
        0xF0,
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
        0x38,
        0x12,
        0x00,
        0x00,
        0x07,
        0x06,
        0x61,
        0x00,
        0x2C,
        0x55,
        0x19,
        0x80,
        0x94,
        0x54,
        0x19,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x5D,
        0xE8,
        0x30,
        0xF2,
        0x3C,
        0xFE,
        0x00,
        0x00,
        0xB5,
        0x03,
        0x30,
        0xEE,
        0x0C,
        0xFF,
        0x00,
        0x00,
        0x4B,
        0xFC,
        0x00,
        0x00,
        0xF5,
        0x00,
        0x00,
        0x00,
        0xB5,
        0x03,
        0xD0,
        0x11,
        0x0C,
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
        0x78,
        0x55,
        0x19,
        0x80,
        0xE0,
        0x54,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0xEB,
        0x60,
        0xF2,
        0x6F,
        0xF4,
        0x00,
        0x00,
        0xF8,
        0xFF,
        0x80,
        0xEF,
        0x7C,
        0xF7,
        0x00,
        0x00,
        0x08,
        0x00,
        0x80,
        0xEF,
        0x84,
        0x08,
        0x00,
        0x00,
        0xF8,
        0xFF,
        0x80,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x80,
        0x10,
        0x84,
        0x08,
        0x00,
        0x00,
        0x02,
        0x10,
        0x00,
        0x00,
        0xF0,
        0xFF,
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
        0x8C,
        0x12,
        0x00,
        0x00,
        0x02,
        0x07,
        0x61,
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
        0x0C,
        0xF7,
        0x00,
        0x00,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF8,
        0xFF,
        0x80,
        0x10,
        0x0C,
        0xF7,
        0x00,
        0x00,
        0xFA,
        0xEF,
        0x00,
        0x00,
        0x0E,
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
        0x00,
        0xC2,
        0x12,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x56,
        0x19,
        0x80,
        0x78,
        0x55,
        0x19,
        0x80,
        0xF8,
        0x30,
        0x07,
        0x80,
        0x40,
        0xEB,
        0xE0,
        0xF4,
        0xE0,
        0x28,
        0x00,
        0x00,
        0xEB,
        0x00,
        0x80,
        0xEF,
        0xCE,
        0xFA,
        0x00,
        0x00,
        0x10,
        0xFF,
        0x80,
        0xEF,
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
        0x00,
        0x10,
        0x00,
        0x00,
        0x52,
        0x11,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x5C,
        0x56,
        0x19,
        0x80,
        0xC4,
        0x55,
        0x00,
        0x00,
        0xF8,
        0x30,
        0x07,
        0x80,
        0xA0,
        0xEB,
        0x00,
        0x00,
        0x40,
        0x29,
        0x00,
        0x00,
        0x11,
        0xFF,
        0x80,
        0xEF,
        0x2E,
        0x05,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xCF,
        0xFA,
        0x00,
        0x00,
        0x11,
        0xFF,
        0x80,
        0x10,
        0x2E,
        0x05,
        0x00,
        0x00,
        0xEC,
        0x00,
        0x80,
        0x10,
        0xCF,
        0xFA,
        0x00,
        0x00,
        0x3F,
        0xF0,
        0x00,
        0x00,
        0x2E,
        0xFD,
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
        0x22,
        0x05,
        0x60,
        0xEF,
        0xA3,
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x5D,
        0xFD,
        0x00,
        0x00,
        0x22,
        0x05,
        0x00,
        0x00,
        0xA3,
        0x02,
        0x00,
        0x00,
        0xDF,
        0xFA,
        0x00,
        0x00,
        0x5D,
        0xFD,
        0x00,
        0x00,
        0xAA,
        0xF8,
        0x00,
        0x00,
        0x43,
        0x0E,
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
        0x8C,
        0x11,
        0x00,
        0x00,
        0x04,
        0x03,
        0x61,
        0x00,
        0xF4,
        0x56,
        0x19,
        0x80,
        0x5C,
        0x56,
        0x00,
        0x00,
        0xF8,
        0x30,
        0x07,
        0x80,
        0xCC,
        0xFB,
        0x40,
        0xF3,
        0x6D,
        0x14,
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
        0x78,
        0x07,
        0x00,
        0x00,
        0xD0,
        0xF1,
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
        0x6F,
        0x11,
        0x00,
        0x00,
        0x03,
        0x04,
        0x61,
        0x00,
        0x40,
        0x57,
        0x00,
        0x00,
        0xA8,
        0x56,
        0x19,
        0x80,
        0xF8,
        0x30,
        0x07,
        0x80,
        0x40,
        0xEC,
        0x60,
        0xF3,
    },
};

static void func_dryfield_night_dilapidated_house_8017DD30(GpCoord* coord, s16 arg1);

/// Entry 1 of the room's two-entry descriptor table, the task that plays a
/// stream. It blanks the display and allocates the auxiliary buffers, looks
/// up the stream slot for the current location with view 0x65 or 0x64
/// (chosen by `Wip_SysFlags.field_0`) and queues CD command 0x61 for it,
/// shows the display once the queue's `field_1FA` is set, and blanks it again
/// when the CD goes idle - or, on the pad's 0x800 flag, early, activating CD
/// phase 1. Once the CD is idle it restores the stream state, clears the
/// image buffers, shows the display again, kills itself and resets the heap.
void func_dryfield_night_dilapidated_house_8017DB20(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state = task->state + 1;
            return;
        case 1:
            key = gGameSession->at4;
            if (Wip_SysFlags.field_0 == 2) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slot         = Stream_FindSlot(key.raw.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 0) & 0xFFFF) == 0) {
                return;
            }
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            taskKill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Entry 0 of the room's two-entry descriptor table: spawns entry 1, the
/// stream-playing task, with an ordering table, sets `gDisplayState.at100.flags.flipMode`, spawns the
/// view tasks and kills itself.
void func_dryfield_night_dilapidated_house_8017DCE0(Task* arg0)
{
    Display_SpawnWithOt(D_dryfield_night_dilapidated_house_801872B4, 1, 0, 0);
    gDisplayState.at100.flags.flipMode = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Draws a four-sided prism from the eight corners at
/// `D_dryfield_night_dilapidated_house_801872CC[arg1..]` as five gouraud
/// `POLY_G4`: four sides joining the lit ring `[0..3]` to the far ring
/// `[4..7]`, then a cap over the lit ring. Each corner is rotated by `coord`'s
/// `workm` and moved by its translation before projection through
/// `GsWSMATRIX`. The lit corners share a pulsing colour whose red is three
/// quarters of its green and blue; the far corners are black.
static void func_dryfield_night_dilapidated_house_8017DD30(GpCoord* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    s16              pulse;
    s16              red;
    s16              blue;
    s16              green;

    pulse = (rsin(gDisplayState.animFrame << 10) >> 12) + 0x10;
    SCRATCH_PUSH(RoomQuadScratch);
    blk = SCRATCH_HEAD(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    red   = pulse * 3 / 4;
    green = pulse;
    blue  = pulse;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx += coord->workm.t[0];
        blk->v[0].vy += coord->workm.t[1];
        blk->v[0].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx += coord->workm.t[0];
        blk->v[1].vy += coord->workm.t[1];
        blk->v[1].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx += coord->workm.t[0];
        blk->v[2].vy += coord->workm.t[1];
        blk->v[2].vz += coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx += coord->workm.t[0];
        blk->v[3].vy += coord->workm.t[1];
        blk->v[3].vz += coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, red, green, blue);
        setRGB1(prim, red, green, blue);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx += coord->workm.t[0];
    blk->v[0].vy += coord->workm.t[1];
    blk->v[0].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx += coord->workm.t[0];
    blk->v[1].vy += coord->workm.t[1];
    blk->v[1].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx += coord->workm.t[0];
    blk->v[2].vy += coord->workm.t[1];
    blk->v[2].vz += coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_night_dilapidated_house_801872CC[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx += coord->workm.t[0];
    blk->v[3].vy += coord->workm.t[1];
    blk->v[3].vz += coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, red, green, blue);
    setRGB1(prim, red, green, blue);
    setRGB2(prim, red, green, blue);
    setRGB3(prim, red, green, blue);
    addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    SCRATCH_POP(RoomQuadScratch);
}

/// Per-frame draw of the room's model task: recomputes the model's world
/// coordinate, then draws up to three prisms, from corner sets 0, 8 and 0x10.
/// Each is gated on `gGameSession->at4.loc.view` taken as a bit index into a
/// fixed mask; the second mask is contained in the other two, so a view in it
/// draws all three.
void func_dryfield_night_dilapidated_house_8017E670(Task* arg0)
{
    GpCoord* coord;
    s32      mask;

    coord = arg0->extra.tmd->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x99C) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0);
    }
    if (mask & 0x998) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 8);
    }
    if (mask & 0x9F8) {
        func_dryfield_night_dilapidated_house_8017DD30(coord, 0x10);
    }
}
