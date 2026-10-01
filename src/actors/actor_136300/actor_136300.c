#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/ending.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_garage.h"
#include "../../shared/screen_wave.h"

// Retained task-shaped record; preserve the callback's actual ABI.
typedef struct {
    u16   flags;
    u16   priority;
    void  (*callback)(Enemy*, Task*);
    void* arg;
} Actor136300RetainedTaskSeed;
STATIC_ASSERT_SIZEOF(Actor136300RetainedTaskSeed, 12);
extern Actor136300RetainedTaskSeed D_actor_136300_8013B11C;

// The engine copies words across the exported animation bank and its
// following argument records. Both views cover the complete backing object.
typedef union {
    struct {
        AnimationSet*        sets[15];
        AnimationPlayRequest arguments[4];
    } data;
    s32 words[35];
} Actor136300AnimCopyB140;
STATIC_ASSERT_SIZEOF(Actor136300AnimCopyB140, 140);

extern Actor136300AnimCopyB140 D_actor_136300_8013B140;

typedef union {
    struct {
        AnimationSet*        sets[26];
        AnimationPlayRequest arguments[2];
    } data;
    s32 words[36];
} Actor136300AnimCopyB2A8;
STATIC_ASSERT_SIZEOF(Actor136300AnimCopyB2A8, 144);

extern Actor136300AnimCopyB2A8 D_actor_136300_8013B2A8;

extern TaskDesc D_actor_136300_8013B134;

extern AnimationPlayRequest D_actor_136300_8013B208;
extern AnimationPlayRequest D_actor_136300_8013B230;

/// Script pair handed to `func_800E8614` -- the first while the ending is being
/// armed, the second when the capture event is cancelled.
extern EvsCommand D_actor_136300_8013C5C8[];
extern EvsCommand D_actor_136300_8013C6C0[];

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running context, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The ramp context the running wave task was spawned with, parked at spawn
/// so the tick reads the ramp through it.
extern OverlayWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern OverlayWaveRec6 gScreenWaveColumns[13];
extern OverlayWaveRec6 gScreenWaveRows[32];

/// The ramp context the message handler seeds and hands to the screen-wave
/// task.
extern OverlayWaveCtx D_actor_136300_8013C99C;

/// Spawn table of the screen-wave task.
extern TaskDesc D_actor_136300_80132AC4[];

extern AnimationSet D_actor_136300_80132D34;
extern AnimationSet D_actor_136300_80132F0C;
extern AnimationSet D_actor_136300_80133328;
extern AnimationSet D_actor_136300_801334E4;
extern AnimationSet D_actor_136300_801336CC;
extern AnimationSet D_actor_136300_80133988;
extern AnimationSet D_actor_136300_80133B68;
extern AnimationSet D_actor_136300_80133FC0;
extern AnimationSet D_actor_136300_801345D8;
extern AnimationSet D_actor_136300_80134F84;
extern AnimationSet D_actor_136300_801352CC;
extern AnimationSet D_actor_136300_80135508;
extern AnimationSet D_actor_136300_80135818;
extern AnimationSet D_actor_136300_80135E94;
extern AnimationSet D_actor_136300_80136478;
void                func_actor_136300_8013267C(Task*);
void                func_actor_136300_80132854(Task*);

extern Actor136300AnimCopyB140 D_actor_136300_8013B140;
extern AnimationPlayRequest    D_actor_136300_8013B1CC;
extern AnimationPlayRequest    D_actor_136300_8013B1E0;
extern AnimationPlayRequest    D_actor_136300_8013B1F4;
extern AnimationSet            D_actor_136300_8013677C;
extern AnimationSet            D_actor_136300_80136980;
extern AnimationSet            D_actor_136300_80136C60;
extern AnimationSet            D_actor_136300_80136E58;
extern AnimationSet            D_actor_136300_801371AC;
extern AnimationSet            D_actor_136300_801373A4;
extern AnimationSet            D_actor_136300_80137568;
extern AnimationSet            D_actor_136300_80137828;
extern AnimationSet            D_actor_136300_801379EC;
extern AnimationSet            D_actor_136300_80137DA4;
extern AnimationSet            D_actor_136300_801380D0;
extern AnimationSet            D_actor_136300_80138388;
extern AnimationSet            D_actor_136300_80138518;
extern AnimationSet            D_actor_136300_801387D8;
extern AnimationSet            D_actor_136300_80138BAC;
extern AnimationSet            D_actor_136300_80138F9C;
extern AnimationSet            D_actor_136300_80139308;
extern AnimationSet            D_actor_136300_80139A04;
extern AnimationSet            D_actor_136300_80139C7C;
extern AnimationSet            D_actor_136300_8013A0D0;
extern AnimationSet            D_actor_136300_8013A364;
extern AnimationSet            D_actor_136300_8013A6CC;
extern AnimationSet            D_actor_136300_8013A8A4;
extern AnimationSet            D_actor_136300_8013ABBC;
extern AnimationSet            D_actor_136300_8013ADB8;
extern AnimationSet            D_actor_136300_8013B0F4;
void                           func_actor_136300_801328D4(s8);
void                           func_actor_136300_801328E0(s32);
void                           func_actor_136300_80132910(s32);
void                           func_actor_136300_80132998(void);
void                           func_actor_136300_801329EC(void);
void                           func_actor_136300_80132A4C(s32);
void                           func_actor_136300_80132A7C(s32);

void func_actor_136300_801328E0(s32);
void func_actor_136300_80132910(s32);

TaskDesc D_actor_136300_80132AC4[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

AnimationPackedPose D_actor_136300_80132AE0[2] = {
#include "assets/actor_136300_animation_00F14_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80132AF8[32] = {
#include "assets/actor_136300_animation_00F14_bank4.inc"
};

AnimationRecord D_actor_136300_80132B78[101] = {
#include "assets/actor_136300_animation_00F14_records.inc"
};

u16 D_actor_136300_80132D0C[20] = {
#include "assets/actor_136300_animation_00F14_indices.inc"
};

AnimationSet D_actor_136300_80132D34 = {
    D_actor_136300_80132B78,
    D_actor_136300_80132D0C,
    { NULL, D_actor_136300_80132AE0, NULL, NULL, D_actor_136300_80132AF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80132D5C[3] = {
#include "assets/actor_136300_animation_010EC_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80132D80[29] = {
#include "assets/actor_136300_animation_010EC_bank4.inc"
};

AnimationRecord D_actor_136300_80132DF4[60] = {
#include "assets/actor_136300_animation_010EC_records.inc"
};

u16 D_actor_136300_80132EE4[20] = {
#include "assets/actor_136300_animation_010EC_indices.inc"
};

AnimationSet D_actor_136300_80132F0C = {
    D_actor_136300_80132DF4,
    D_actor_136300_80132EE4,
    { NULL, D_actor_136300_80132D5C, NULL, NULL, D_actor_136300_80132D80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80132F34[3] = {
#include "assets/actor_136300_animation_01508_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80132F58[80] = {
#include "assets/actor_136300_animation_01508_bank4.inc"
};

AnimationRecord D_actor_136300_80133098[154] = {
#include "assets/actor_136300_animation_01508_records.inc"
};

u16 D_actor_136300_80133300[20] = {
#include "assets/actor_136300_animation_01508_indices.inc"
};

AnimationSet D_actor_136300_80133328 = {
    D_actor_136300_80133098,
    D_actor_136300_80133300,
    { NULL, D_actor_136300_80132F34, NULL, NULL, D_actor_136300_80132F58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80133350[2] = {
#include "assets/actor_136300_animation_016C4_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80133368[25] = {
#include "assets/actor_136300_animation_016C4_bank4.inc"
};

AnimationRecord D_actor_136300_801333CC[60] = {
#include "assets/actor_136300_animation_016C4_records.inc"
};

u16 D_actor_136300_801334BC[20] = {
#include "assets/actor_136300_animation_016C4_indices.inc"
};

AnimationSet D_actor_136300_801334E4 = {
    D_actor_136300_801333CC,
    D_actor_136300_801334BC,
    { NULL, D_actor_136300_80133350, NULL, NULL, D_actor_136300_80133368, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013350C[3] = {
#include "assets/actor_136300_animation_018AC_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80133530[33] = {
#include "assets/actor_136300_animation_018AC_bank4.inc"
};

AnimationRecord D_actor_136300_801335B4[60] = {
#include "assets/actor_136300_animation_018AC_records.inc"
};

u16 D_actor_136300_801336A4[20] = {
#include "assets/actor_136300_animation_018AC_indices.inc"
};

AnimationSet D_actor_136300_801336CC = {
    D_actor_136300_801335B4,
    D_actor_136300_801336A4,
    { NULL, D_actor_136300_8013350C, NULL, NULL, D_actor_136300_80133530, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801336F4[3] = {
#include "assets/actor_136300_animation_01B68_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80133718[39] = {
#include "assets/actor_136300_animation_01B68_bank4.inc"
};

AnimationRecord D_actor_136300_801337B4[107] = {
#include "assets/actor_136300_animation_01B68_records.inc"
};

u16 D_actor_136300_80133960[20] = {
#include "assets/actor_136300_animation_01B68_indices.inc"
};

AnimationSet D_actor_136300_80133988 = {
    D_actor_136300_801337B4,
    D_actor_136300_80133960,
    { NULL, D_actor_136300_801336F4, NULL, NULL, D_actor_136300_80133718, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801339B0[3] = {
#include "assets/actor_136300_animation_01D48_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801339D4[31] = {
#include "assets/actor_136300_animation_01D48_bank4.inc"
};

AnimationRecord D_actor_136300_80133A50[60] = {
#include "assets/actor_136300_animation_01D48_records.inc"
};

u16 D_actor_136300_80133B40[20] = {
#include "assets/actor_136300_animation_01D48_indices.inc"
};

AnimationSet D_actor_136300_80133B68 = {
    D_actor_136300_80133A50,
    D_actor_136300_80133B40,
    { NULL, D_actor_136300_801339B0, NULL, NULL, D_actor_136300_801339D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80133B90[7] = {
#include "assets/actor_136300_animation_021A0_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80133BE4[85] = {
#include "assets/actor_136300_animation_021A0_bank4.inc"
};

AnimationRecord D_actor_136300_80133D38[152] = {
#include "assets/actor_136300_animation_021A0_records.inc"
};

u16 D_actor_136300_80133F98[20] = {
#include "assets/actor_136300_animation_021A0_indices.inc"
};

AnimationSet D_actor_136300_80133FC0 = {
    D_actor_136300_80133D38,
    D_actor_136300_80133F98,
    { NULL, D_actor_136300_80133B90, NULL, NULL, D_actor_136300_80133BE4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80133FE8[4] = {
#include "assets/actor_136300_animation_027B8_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80134018[147] = {
#include "assets/actor_136300_animation_027B8_bank4.inc"
};

AnimationRecord D_actor_136300_80134264[211] = {
#include "assets/actor_136300_animation_027B8_records.inc"
};

u16 D_actor_136300_801345B0[20] = {
#include "assets/actor_136300_animation_027B8_indices.inc"
};

AnimationSet D_actor_136300_801345D8 = {
    D_actor_136300_80134264,
    D_actor_136300_801345B0,
    { NULL, D_actor_136300_80133FE8, NULL, NULL, D_actor_136300_80134018, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80134600[9] = {
#include "assets/actor_136300_animation_03164_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013466C[245] = {
#include "assets/actor_136300_animation_03164_bank4.inc"
};

AnimationRecord D_actor_136300_80134A40[327] = {
#include "assets/actor_136300_animation_03164_records.inc"
};

u16 D_actor_136300_80134F5C[20] = {
#include "assets/actor_136300_animation_03164_indices.inc"
};

AnimationSet D_actor_136300_80134F84 = {
    D_actor_136300_80134A40,
    D_actor_136300_80134F5C,
    { NULL, D_actor_136300_80134600, NULL, NULL, D_actor_136300_8013466C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80134FAC[6] = {
#include "assets/actor_136300_animation_034AC_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80134FF4[69] = {
#include "assets/actor_136300_animation_034AC_bank4.inc"
};

AnimationRecord D_actor_136300_80135108[103] = {
#include "assets/actor_136300_animation_034AC_records.inc"
};

u16 D_actor_136300_801352A4[20] = {
#include "assets/actor_136300_animation_034AC_indices.inc"
};

AnimationSet D_actor_136300_801352CC = {
    D_actor_136300_80135108,
    D_actor_136300_801352A4,
    { NULL, D_actor_136300_80134FAC, NULL, NULL, D_actor_136300_80134FF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801352F4[3] = {
#include "assets/actor_136300_animation_036E8_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80135318[24] = {
#include "assets/actor_136300_animation_036E8_bank4.inc"
};

AnimationRecord D_actor_136300_80135378[90] = {
#include "assets/actor_136300_animation_036E8_records.inc"
};

u16 D_actor_136300_801354E0[20] = {
#include "assets/actor_136300_animation_036E8_indices.inc"
};

AnimationSet D_actor_136300_80135508 = {
    D_actor_136300_80135378,
    D_actor_136300_801354E0,
    { NULL, D_actor_136300_801352F4, NULL, NULL, D_actor_136300_80135318, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80135530[6] = {
#include "assets/actor_136300_animation_039F8_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80135578[61] = {
#include "assets/actor_136300_animation_039F8_bank4.inc"
};

AnimationRecord D_actor_136300_8013566C[97] = {
#include "assets/actor_136300_animation_039F8_records.inc"
};

u16 D_actor_136300_801357F0[20] = {
#include "assets/actor_136300_animation_039F8_indices.inc"
};

AnimationSet D_actor_136300_80135818 = {
    D_actor_136300_8013566C,
    D_actor_136300_801357F0,
    { NULL, D_actor_136300_80135530, NULL, NULL, D_actor_136300_80135578, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80135840[10] = {
#include "assets/actor_136300_animation_04074_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801358B8[142] = {
#include "assets/actor_136300_animation_04074_bank4.inc"
};

AnimationRecord D_actor_136300_80135AF0[223] = {
#include "assets/actor_136300_animation_04074_records.inc"
};

u16 D_actor_136300_80135E6C[20] = {
#include "assets/actor_136300_animation_04074_indices.inc"
};

AnimationSet D_actor_136300_80135E94 = {
    D_actor_136300_80135AF0,
    D_actor_136300_80135E6C,
    { NULL, D_actor_136300_80135840, NULL, NULL, D_actor_136300_801358B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80135EBC[2] = {
#include "assets/actor_136300_animation_04658_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80135ED4[152] = {
#include "assets/actor_136300_animation_04658_bank4.inc"
};

AnimationRecord D_actor_136300_80136134[199] = {
#include "assets/actor_136300_animation_04658_records.inc"
};

u16 D_actor_136300_80136450[20] = {
#include "assets/actor_136300_animation_04658_indices.inc"
};

AnimationSet D_actor_136300_80136478 = {
    D_actor_136300_80136134,
    D_actor_136300_80136450,
    { NULL, D_actor_136300_80135EBC, NULL, NULL, D_actor_136300_80135ED4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801364A0[6] = {
#include "assets/actor_136300_animation_0495C_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801364E8[46] = {
#include "assets/actor_136300_animation_0495C_bank4.inc"
};

AnimationRecord D_actor_136300_801365A0[109] = {
#include "assets/actor_136300_animation_0495C_records.inc"
};

u16 D_actor_136300_80136754[20] = {
#include "assets/actor_136300_animation_0495C_indices.inc"
};

AnimationSet D_actor_136300_8013677C = {
    D_actor_136300_801365A0,
    D_actor_136300_80136754,
    { NULL, D_actor_136300_801364A0, NULL, NULL, D_actor_136300_801364E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801367A4[2] = {
#include "assets/actor_136300_animation_04B60_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801367BC[21] = {
#include "assets/actor_136300_animation_04B60_bank4.inc"
};

AnimationRecord D_actor_136300_80136810[82] = {
#include "assets/actor_136300_animation_04B60_records.inc"
};

u16 D_actor_136300_80136958[20] = {
#include "assets/actor_136300_animation_04B60_indices.inc"
};

AnimationSet D_actor_136300_80136980 = {
    D_actor_136300_80136810,
    D_actor_136300_80136958,
    { NULL, D_actor_136300_801367A4, NULL, NULL, D_actor_136300_801367BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801369A8[5] = {
#include "assets/actor_136300_animation_04E40_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801369E4[61] = {
#include "assets/actor_136300_animation_04E40_bank4.inc"
};

AnimationRecord D_actor_136300_80136AD8[88] = {
#include "assets/actor_136300_animation_04E40_records.inc"
};

u16 D_actor_136300_80136C38[20] = {
#include "assets/actor_136300_animation_04E40_indices.inc"
};

AnimationSet D_actor_136300_80136C60 = {
    D_actor_136300_80136AD8,
    D_actor_136300_80136C38,
    { NULL, D_actor_136300_801369A8, NULL, NULL, D_actor_136300_801369E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80136C88[3] = {
#include "assets/actor_136300_animation_05038_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80136CAC[34] = {
#include "assets/actor_136300_animation_05038_bank4.inc"
};

AnimationRecord D_actor_136300_80136D34[63] = {
#include "assets/actor_136300_animation_05038_records.inc"
};

u16 D_actor_136300_80136E30[20] = {
#include "assets/actor_136300_animation_05038_indices.inc"
};

AnimationSet D_actor_136300_80136E58 = {
    D_actor_136300_80136D34,
    D_actor_136300_80136E30,
    { NULL, D_actor_136300_80136C88, NULL, NULL, D_actor_136300_80136CAC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80136E80[5] = {
#include "assets/actor_136300_animation_0538C_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80136EBC[58] = {
#include "assets/actor_136300_animation_0538C_bank4.inc"
};

AnimationRecord D_actor_136300_80136FA4[120] = {
#include "assets/actor_136300_animation_0538C_records.inc"
};

u16 D_actor_136300_80137184[20] = {
#include "assets/actor_136300_animation_0538C_indices.inc"
};

AnimationSet D_actor_136300_801371AC = {
    D_actor_136300_80136FA4,
    D_actor_136300_80137184,
    { NULL, D_actor_136300_80136E80, NULL, NULL, D_actor_136300_80136EBC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801371D4[3] = {
#include "assets/actor_136300_animation_05584_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801371F8[34] = {
#include "assets/actor_136300_animation_05584_bank4.inc"
};

AnimationRecord D_actor_136300_80137280[63] = {
#include "assets/actor_136300_animation_05584_records.inc"
};

u16 D_actor_136300_8013737C[20] = {
#include "assets/actor_136300_animation_05584_indices.inc"
};

AnimationSet D_actor_136300_801373A4 = {
    D_actor_136300_80137280,
    D_actor_136300_8013737C,
    { NULL, D_actor_136300_801371D4, NULL, NULL, D_actor_136300_801371F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801373CC[3] = {
#include "assets/actor_136300_animation_05748_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801373F0[27] = {
#include "assets/actor_136300_animation_05748_bank4.inc"
};

AnimationRecord D_actor_136300_8013745C[57] = {
#include "assets/actor_136300_animation_05748_records.inc"
};

u16 D_actor_136300_80137540[20] = {
#include "assets/actor_136300_animation_05748_indices.inc"
};

AnimationSet D_actor_136300_80137568 = {
    D_actor_136300_8013745C,
    D_actor_136300_80137540,
    { NULL, D_actor_136300_801373CC, NULL, NULL, D_actor_136300_801373F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80137590[4] = {
#include "assets/actor_136300_animation_05A08_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801375C0[40] = {
#include "assets/actor_136300_animation_05A08_bank4.inc"
};

AnimationRecord D_actor_136300_80137660[104] = {
#include "assets/actor_136300_animation_05A08_records.inc"
};

u16 D_actor_136300_80137800[20] = {
#include "assets/actor_136300_animation_05A08_indices.inc"
};

AnimationSet D_actor_136300_80137828 = {
    D_actor_136300_80137660,
    D_actor_136300_80137800,
    { NULL, D_actor_136300_80137590, NULL, NULL, D_actor_136300_801375C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80137850[3] = {
#include "assets/actor_136300_animation_05BCC_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80137874[27] = {
#include "assets/actor_136300_animation_05BCC_bank4.inc"
};

AnimationRecord D_actor_136300_801378E0[57] = {
#include "assets/actor_136300_animation_05BCC_records.inc"
};

u16 D_actor_136300_801379C4[20] = {
#include "assets/actor_136300_animation_05BCC_indices.inc"
};

AnimationSet D_actor_136300_801379EC = {
    D_actor_136300_801378E0,
    D_actor_136300_801379C4,
    { NULL, D_actor_136300_80137850, NULL, NULL, D_actor_136300_80137874, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80137A14[3] = {
#include "assets/actor_136300_animation_05F84_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80137A38[81] = {
#include "assets/actor_136300_animation_05F84_bank4.inc"
};

AnimationRecord D_actor_136300_80137B7C[128] = {
#include "assets/actor_136300_animation_05F84_records.inc"
};

u16 D_actor_136300_80137D7C[20] = {
#include "assets/actor_136300_animation_05F84_indices.inc"
};

AnimationSet D_actor_136300_80137DA4 = {
    D_actor_136300_80137B7C,
    D_actor_136300_80137D7C,
    { NULL, D_actor_136300_80137A14, NULL, NULL, D_actor_136300_80137A38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80137DCC[7] = {
#include "assets/actor_136300_animation_062B0_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80137E20[56] = {
#include "assets/actor_136300_animation_062B0_bank4.inc"
};

AnimationRecord D_actor_136300_80137F00[106] = {
#include "assets/actor_136300_animation_062B0_records.inc"
};

u16 D_actor_136300_801380A8[20] = {
#include "assets/actor_136300_animation_062B0_indices.inc"
};

AnimationSet D_actor_136300_801380D0 = {
    D_actor_136300_80137F00,
    D_actor_136300_801380A8,
    { NULL, D_actor_136300_80137DCC, NULL, NULL, D_actor_136300_80137E20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801380F8[4] = {
#include "assets/actor_136300_animation_06568_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80138128[57] = {
#include "assets/actor_136300_animation_06568_bank4.inc"
};

AnimationRecord D_actor_136300_8013820C[85] = {
#include "assets/actor_136300_animation_06568_records.inc"
};

u16 D_actor_136300_80138360[20] = {
#include "assets/actor_136300_animation_06568_indices.inc"
};

AnimationSet D_actor_136300_80138388 = {
    D_actor_136300_8013820C,
    D_actor_136300_80138360,
    { NULL, D_actor_136300_801380F8, NULL, NULL, D_actor_136300_80138128, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_801383B0[2] = {
#include "assets/actor_136300_animation_066F8_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801383C8[17] = {
#include "assets/actor_136300_animation_066F8_bank4.inc"
};

AnimationRecord D_actor_136300_8013840C[57] = {
#include "assets/actor_136300_animation_066F8_records.inc"
};

u16 D_actor_136300_801384F0[20] = {
#include "assets/actor_136300_animation_066F8_indices.inc"
};

AnimationSet D_actor_136300_80138518 = {
    D_actor_136300_8013840C,
    D_actor_136300_801384F0,
    { NULL, D_actor_136300_801383B0, NULL, NULL, D_actor_136300_801383C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80138540[5] = {
#include "assets/actor_136300_animation_069B8_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013857C[56] = {
#include "assets/actor_136300_animation_069B8_bank4.inc"
};

AnimationRecord D_actor_136300_8013865C[85] = {
#include "assets/actor_136300_animation_069B8_records.inc"
};

u16 D_actor_136300_801387B0[20] = {
#include "assets/actor_136300_animation_069B8_indices.inc"
};

AnimationSet D_actor_136300_801387D8 = {
    D_actor_136300_8013865C,
    D_actor_136300_801387B0,
    { NULL, D_actor_136300_80138540, NULL, NULL, D_actor_136300_8013857C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80138800[8] = {
#include "assets/actor_136300_animation_06D8C_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80138860[84] = {
#include "assets/actor_136300_animation_06D8C_bank4.inc"
};

AnimationRecord D_actor_136300_801389B0[117] = {
#include "assets/actor_136300_animation_06D8C_records.inc"
};

u16 D_actor_136300_80138B84[20] = {
#include "assets/actor_136300_animation_06D8C_indices.inc"
};

AnimationSet D_actor_136300_80138BAC = {
    D_actor_136300_801389B0,
    D_actor_136300_80138B84,
    { NULL, D_actor_136300_80138800, NULL, NULL, D_actor_136300_80138860, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80138BD4[7] = {
#include "assets/actor_136300_animation_0717C_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80138C28[62] = {
#include "assets/actor_136300_animation_0717C_bank4.inc"
};

AnimationRecord D_actor_136300_80138D20[149] = {
#include "assets/actor_136300_animation_0717C_records.inc"
};

u16 D_actor_136300_80138F74[20] = {
#include "assets/actor_136300_animation_0717C_indices.inc"
};

AnimationSet D_actor_136300_80138F9C = {
    D_actor_136300_80138D20,
    D_actor_136300_80138F74,
    { NULL, D_actor_136300_80138BD4, NULL, NULL, D_actor_136300_80138C28, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80138FC4[7] = {
#include "assets/actor_136300_animation_074E8_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80139018[74] = {
#include "assets/actor_136300_animation_074E8_bank4.inc"
};

AnimationRecord D_actor_136300_80139140[104] = {
#include "assets/actor_136300_animation_074E8_records.inc"
};

u16 D_actor_136300_801392E0[20] = {
#include "assets/actor_136300_animation_074E8_indices.inc"
};

AnimationSet D_actor_136300_80139308 = {
    D_actor_136300_80139140,
    D_actor_136300_801392E0,
    { NULL, D_actor_136300_80138FC4, NULL, NULL, D_actor_136300_80139018, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80139330[10] = {
#include "assets/actor_136300_animation_07BE4_bank1.inc"
};

AnimationPackedRotation D_actor_136300_801393A8[147] = {
#include "assets/actor_136300_animation_07BE4_bank4.inc"
};

AnimationRecord D_actor_136300_801395F4[250] = {
#include "assets/actor_136300_animation_07BE4_records.inc"
};

u16 D_actor_136300_801399DC[20] = {
#include "assets/actor_136300_animation_07BE4_indices.inc"
};

AnimationSet D_actor_136300_80139A04 = {
    D_actor_136300_801395F4,
    D_actor_136300_801399DC,
    { NULL, D_actor_136300_80139330, NULL, NULL, D_actor_136300_801393A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80139A2C[4] = {
#include "assets/actor_136300_animation_07E5C_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80139A5C[51] = {
#include "assets/actor_136300_animation_07E5C_bank4.inc"
};

AnimationRecord D_actor_136300_80139B28[75] = {
#include "assets/actor_136300_animation_07E5C_records.inc"
};

u16 D_actor_136300_80139C54[20] = {
#include "assets/actor_136300_animation_07E5C_indices.inc"
};

AnimationSet D_actor_136300_80139C7C = {
    D_actor_136300_80139B28,
    D_actor_136300_80139C54,
    { NULL, D_actor_136300_80139A2C, NULL, NULL, D_actor_136300_80139A5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_80139CA4[5] = {
#include "assets/actor_136300_animation_082B0_bank1.inc"
};

AnimationPackedRotation D_actor_136300_80139CE0[80] = {
#include "assets/actor_136300_animation_082B0_bank4.inc"
};

AnimationRecord D_actor_136300_80139E20[162] = {
#include "assets/actor_136300_animation_082B0_records.inc"
};

u16 D_actor_136300_8013A0A8[20] = {
#include "assets/actor_136300_animation_082B0_indices.inc"
};

AnimationSet D_actor_136300_8013A0D0 = {
    D_actor_136300_80139E20,
    D_actor_136300_8013A0A8,
    { NULL, D_actor_136300_80139CA4, NULL, NULL, D_actor_136300_80139CE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013A0F8[4] = {
#include "assets/actor_136300_animation_08544_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013A128[51] = {
#include "assets/actor_136300_animation_08544_bank4.inc"
};

AnimationRecord D_actor_136300_8013A1F4[82] = {
#include "assets/actor_136300_animation_08544_records.inc"
};

u16 D_actor_136300_8013A33C[20] = {
#include "assets/actor_136300_animation_08544_indices.inc"
};

AnimationSet D_actor_136300_8013A364 = {
    D_actor_136300_8013A1F4,
    D_actor_136300_8013A33C,
    { NULL, D_actor_136300_8013A0F8, NULL, NULL, D_actor_136300_8013A128, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013A38C[5] = {
#include "assets/actor_136300_animation_088AC_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013A3C8[74] = {
#include "assets/actor_136300_animation_088AC_bank4.inc"
};

AnimationRecord D_actor_136300_8013A4F0[109] = {
#include "assets/actor_136300_animation_088AC_records.inc"
};

u16 D_actor_136300_8013A6A4[20] = {
#include "assets/actor_136300_animation_088AC_indices.inc"
};

AnimationSet D_actor_136300_8013A6CC = {
    D_actor_136300_8013A4F0,
    D_actor_136300_8013A6A4,
    { NULL, D_actor_136300_8013A38C, NULL, NULL, D_actor_136300_8013A3C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013A6F4[2] = {
#include "assets/actor_136300_animation_08A84_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013A70C[16] = {
#include "assets/actor_136300_animation_08A84_bank4.inc"
};

AnimationRecord D_actor_136300_8013A74C[76] = {
#include "assets/actor_136300_animation_08A84_records.inc"
};

u16 D_actor_136300_8013A87C[20] = {
#include "assets/actor_136300_animation_08A84_indices.inc"
};

AnimationSet D_actor_136300_8013A8A4 = {
    D_actor_136300_8013A74C,
    D_actor_136300_8013A87C,
    { NULL, D_actor_136300_8013A6F4, NULL, NULL, D_actor_136300_8013A70C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013A8CC[5] = {
#include "assets/actor_136300_animation_08D9C_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013A908[66] = {
#include "assets/actor_136300_animation_08D9C_bank4.inc"
};

AnimationRecord D_actor_136300_8013AA10[97] = {
#include "assets/actor_136300_animation_08D9C_records.inc"
};

u16 D_actor_136300_8013AB94[20] = {
#include "assets/actor_136300_animation_08D9C_indices.inc"
};

AnimationSet D_actor_136300_8013ABBC = {
    D_actor_136300_8013AA10,
    D_actor_136300_8013AB94,
    { NULL, D_actor_136300_8013A8CC, NULL, NULL, D_actor_136300_8013A908, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013ABE4[4] = {
#include "assets/actor_136300_animation_08F98_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013AC14[17] = {
#include "assets/actor_136300_animation_08F98_bank4.inc"
};

AnimationRecord D_actor_136300_8013AC58[78] = {
#include "assets/actor_136300_animation_08F98_records.inc"
};

u16 D_actor_136300_8013AD90[20] = {
#include "assets/actor_136300_animation_08F98_indices.inc"
};

AnimationSet D_actor_136300_8013ADB8 = {
    D_actor_136300_8013AC58,
    D_actor_136300_8013AD90,
    { NULL, D_actor_136300_8013ABE4, NULL, NULL, D_actor_136300_8013AC14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_136300_8013ADE0[8] = {
#include "assets/actor_136300_animation_092D4_bank1.inc"
};

AnimationPackedRotation D_actor_136300_8013AE40[65] = {
#include "assets/actor_136300_animation_092D4_bank4.inc"
};

AnimationRecord D_actor_136300_8013AF44[98] = {
#include "assets/actor_136300_animation_092D4_records.inc"
};

u16 D_actor_136300_8013B0CC[20] = {
#include "assets/actor_136300_animation_092D4_indices.inc"
};

AnimationSet D_actor_136300_8013B0F4 = {
    D_actor_136300_8013AF44,
    D_actor_136300_8013B0CC,
    { NULL, D_actor_136300_8013ADE0, NULL, NULL, D_actor_136300_8013AE40, NULL, NULL, NULL },
};

Actor136300RetainedTaskSeed D_actor_136300_8013B11C = { 0, 192, Gp_DestroyEnemy, NULL };

TaskDesc D_actor_136300_8013B128 = { { { TASK_BODY_NONE, 32 } }, func_actor_136300_8013267C, { .value = 0 } };

TaskDesc D_actor_136300_8013B134 = { { { TASK_BODY_NONE, 32 } }, func_actor_136300_80132854, { .value = 0 } };

Actor136300AnimCopyB140 D_actor_136300_8013B140 = { .data = { { &D_actor_136300_80132D34, &D_actor_136300_80132F0C, &D_actor_136300_80133328, &D_actor_136300_801334E4, &D_actor_136300_801336CC, &D_actor_136300_80133988, &D_actor_136300_80133B68, &D_actor_136300_80133FC0, &D_actor_136300_801345D8, &D_actor_136300_80134F84, &D_actor_136300_801352CC, &D_actor_136300_80135508, &D_actor_136300_80135818, &D_actor_136300_80135E94, &D_actor_136300_80136478 }, { { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_136300_8013B1CC = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B1E0 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B1F4 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B208 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B21C = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B230 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B244 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B258 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B26C = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B280 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B294 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

Actor136300AnimCopyB2A8 D_actor_136300_8013B2A8 = { .data = { { &D_actor_136300_8013677C, &D_actor_136300_80136980, &D_actor_136300_80136C60, &D_actor_136300_80136E58, &D_actor_136300_801371AC, &D_actor_136300_801373A4, &D_actor_136300_80137568, &D_actor_136300_80137828, &D_actor_136300_801379EC, &D_actor_136300_80137DA4, &D_actor_136300_801380D0, &D_actor_136300_80138388, &D_actor_136300_80138518, &D_actor_136300_801387D8, &D_actor_136300_80138BAC, &D_actor_136300_80138F9C, &D_actor_136300_80139308, &D_actor_136300_80139A04, &D_actor_136300_80139C7C, &D_actor_136300_8013A0D0, &D_actor_136300_8013A364, &D_actor_136300_8013A6CC, &D_actor_136300_8013A8A4, &D_actor_136300_8013ABBC, &D_actor_136300_8013ADB8, &D_actor_136300_8013B0F4 }, { { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

AnimationPlayRequest D_actor_136300_8013B338 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B34C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B360 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B374 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B388 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B39C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3B0 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3C4 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3D8 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B3EC = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B400 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B414 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B428 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B43C = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B450 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B464 = { { .index = 1 }, 64, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B478 = { { .index = 1 }, 65, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B48C = { { .index = 1 }, 66, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4A0 = { { .index = 1 }, 67, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4B4 = { { .index = 1 }, 68, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4C8 = { { .index = 1 }, 69, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4DC = { { .index = 1 }, 70, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B4F0 = { { .index = 1 }, 71, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_136300_8013B504 = { { .index = 1 }, 72, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpCopyArg D_actor_136300_8013B518 = { { .words = D_actor_136300_8013B2A8.words }, 32 };

GpCopyArg D_actor_136300_8013B520 = { { .words = D_actor_136300_8013B140.words }, 32 };

ActorTransform D_actor_136300_8013B528 = { { 2000, 0, 1940, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_136300_8013B540 = { { 2000, 0, 3940, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_136300_8013B558 = { { 2000, 0, 5240, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_136300_8013B570 = { { 2230, 0, 5240, 0 }, { 0, -1024, 0, 0 } };

GpOverrideArg D_actor_136300_8013B588 = { 19, 47 };

EvsCommand D_actor_136300_8013B590[149] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_136300_8013B518 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_136300_8013B520 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_136300_8013B2A8.data.arguments }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_136300_8013B140.data.arguments }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132910 }, { .value = -2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_136300_8013B528 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1010 }, { .storage = &D_actor_136300_8013B540 }, { .storage = &D_actor_136300_8013B588 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_136300_8013B558 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B34C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B360 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B374 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B230 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3EC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B400 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B414 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B244 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B258 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B26C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B280 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B428 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B43C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B450 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4C8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4DC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A4C }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_801328E0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_136300_8013B518 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_136300_8013B520 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_136300_8013B540 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_136300_8013B558 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4F0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B504 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B48C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B4A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B294 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B464 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B244 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B258 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B478 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B1F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B21C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_136300_801328D4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_136300_8013B570 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_136300_8013C388[16] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_136300_801328D4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_actor_136300_8013B570 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B140.data.arguments[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_136300_8013C508[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132A7C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_136300_8013C5C8[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_136300_8013B518 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_136300_8013B520 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136300_80132998 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_136300_8013B3C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136300_801329EC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsSceneKey D_actor_136300_8013C6B8 = { 3, 65, 11 };

EvsCommand D_actor_136300_8013C6C0[8] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_801328E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_136300_8013C780[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136300_80132910 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

OverlayWaveCtx* gScreenWaveCtx = NULL;

OverlayWaveRec6 gScreenWaveColumns[13] = { 0 };

OverlayWaveRec6 gScreenWaveRows[32] = { 0 };

OverlayWaveCtx D_actor_136300_8013C99C = { 0 };

#include "../../shared/screen_wave.inc.c"

/// State machine for the capture-event actor: arms the ending, waits for the
/// capture key, then hands control to the boot loader and spawns the drop-in
/// task. The two `func_800E8614` calls and the `arg0->state += 1` blocks are
/// written out in every arm that needs them; jump optimization merges the
/// identical tails, so one copy of the increment lands between case 3 and case
/// 6 and one copy of the call lands after case 0. Hoisting either tail into a
/// shared `goto` target compiles to a different allocation - the call's address
/// then reaches `$a0` through `$v0` instead of being built there directly.
void func_actor_136300_8013267C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gGameSession->hideHud = 1;
            Gp_MsgPlayerWeapon(0);
            func_800E8614(D_actor_136300_8013C5C8, 1);
            arg0->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                arg0->state += 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 2) {
                gGameSession->hideHud = 0;
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
                return;
            }
            func_800E8614(D_actor_136300_8013C6C0, 1);
            arg0->state += 1;
            return;
        case 3:
            if (gGameSession->eventState != 1) {
                arg0->state += 1;
            }
            return;
        case 4:
        case 5:
            arg0->state += 1;
            return;
        case 6:
            SetDispMask(1);
            SndEvt_EnqueueType7(0x80000000, 0);
            GameFlag_SetNibble(0x7A, 4);
            GameFlag_SetNibble(0x97, 0);
            GameFlag_SetNibble(0x98, 1);
            GameFlag_SetNibble(0x9A, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xF);
            GameFlag_SetNibble(0x4C, 4);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = 9;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = 4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
            Fs_BeginBootLoad((u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 0);
            Gp_ClearCollectedBit(0x116);
            gDisplayState.spriteVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            return;
    }
}

/// Runs once on spawn, then counts `spawnArg1` down; when it goes negative the
/// ending flag is set and the task kills itself. The decrement is one reused
/// local: m2c's temp plus per-arm subtract splits the value into three
/// quantities and the store lands in `$v1` instead of `$v0`.
void func_actor_136300_80132854(Task* arg0)
{
    s32 var_v0;

    if (arg0->state == 0) {
        Gp_SpawnScript18(&D_80114A24, D_80114A34);
        arg0->state += 1;
    }
    var_v0 = arg0->spawnArg1.value;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1.value;
    }
    var_v0                = var_v0 - 1;
    arg0->spawnArg1.value = var_v0;
}

void func_actor_136300_801328D4(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_actor_136300_801328E0(s32 arg0)
{
    Task_SpawnFromTable(D_dryfield_night_garage_80183380, 0, arg0, 0);
}

/// Message handler driving the screen wave. A positive argument is written
/// into the wave's ramp state (1 ramps the wave back down, after which the
/// task ends). Otherwise the CD command queue's `field_22A` is set to 2 and,
/// except for the -2 message, the ramp context is seeded (span 0x64 for 0,
/// 5 otherwise, scale 0x100) and the screen-wave task
/// `D_actor_136300_80132AC4` is spawned with it.
///
/// Both halves of the context are written in *each* arm of the span test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the span collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting the scale store out of the arms compiles to a different
/// allocation.
void func_actor_136300_80132910(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    if (arg0 <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_actor_136300_8013C99C.span  = 0x64;
                D_actor_136300_8013C99C.scale = 0x100;
            } else {
                D_actor_136300_8013C99C.span  = 5;
                D_actor_136300_8013C99C.scale = 0x100;
            }
            Task_SpawnFromTable(D_actor_136300_80132AC4, 0, 0, &D_actor_136300_8013C99C);
        }
    } else {
        D_actor_136300_8013C99C.state = arg0;
    }
}

void func_actor_136300_80132998(void)
{
    s32 temp_v0;

    temp_v0 = GameFlag_GetNibble(0x72);
    Gp_StartCapSlot((s16)(temp_v0 + 0x10), 0, 0);
    if (temp_v0 < 2) {
        GameFlag_SetNibble(0x72, temp_v0 + 1);
    }
}

void func_actor_136300_801329EC(void)
{
    AnimationPlayRequest* var_s0;

    if (Gp_GetCapEventKey() == 1) {
        var_s0 = &D_actor_136300_8013B208;
    } else {
        var_s0 = &D_actor_136300_8013B230;
    }
    Gp_AllyAnimId(&var_s0->source.index);
    Gp_DispatchMsgPtr(gameGetPtrSlot(0xA), ANIMATION_MESSAGE_PLAY, var_s0, 0);
}

void func_actor_136300_80132A4C(s32 arg0)
{
    Display_InitModeObj(&D_actor_136300_8013B134, arg0, 0, 0x100);
}

void func_actor_136300_80132A7C(s32 arg0)
{
    if (arg0 == 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x180, 0x100);
        return;
    }
    Gp_ResetCap();
}
